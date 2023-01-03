#pragma once

#include <fmt/core.h>
#include <ctime>

enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

#ifndef _WIN32
inline static void __debugbreak(void)
{
    asm volatile("int $0x03");
}
#endif

struct Logger {
    const char *level_strings[6] = {
            "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
    };
    const char *level_colors[6] = {
        "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
    };

    FILE* file = nullptr;
    int min_level = 0;

    void init(const char* filename) {
        if (fopen_s(&file, filename, "w") != 0) {
            fmt::print("Cannot create log file {}!\n", filename);
        }
    }

    void release() {
        if (file) {
            fclose(file);
        }
    }

    void set_minimum_level(int min_level) {
        this->min_level = min_level;
    }

    template <class... Args>
    void log(int level, const char *filename, int line, std::string_view fmtstr, Args&&... args) {
        time_t cur_time = time(NULL);
        tm cur_localtime;
        #ifdef _WIN32 // REEEEE Microsoft, doesn't adhere to C11
        if (localtime_s(&cur_localtime, &cur_time) == EINVAL) {
            return;
        }
        #else
        if (!localtime_s(&cur_time, &cur_localtime)) {
            return;
        }
        #endif
        char buf[16];
        if (level >= min_level) {
            buf[strftime(buf, sizeof(buf), "%H:%M:%S", &cur_localtime)] = '\0';
            fmt::print("{} {}{:5s}\x1b[0m \x1b[90m{}:{}:\x1b[0m ",
                       buf, level_colors[level], level_strings[level],
                       filename, line);
            fmt::print(fmt::runtime(fmtstr), std::forward<Args>(args)...);
            fmt::print("\n");
            fflush(stdout);
        }
        if (file) {
            char buf_big[64];
            buf[strftime(buf_big, sizeof(buf_big), "%Y-%m-%d %H:%M:%S", &cur_localtime)] = '\0';
            fmt::print(file, "{} {:5s} {}:{}: ", buf_big, level_strings[level], filename, line);
            fmt::print(file, fmt::runtime(fmtstr), std::forward<Args>(args)...);
            fmt::print(file, "\n");
            fflush(file);
        }
    }

    void log_raw(int level, const char* filename, int line, std::string_view str) {
        time_t cur_time = time(NULL);
        tm cur_localtime;
        #ifdef _WIN32 // REEEEE Microsoft, doesn't adhere to C11
        if (localtime_s(&cur_localtime, &cur_time) == EINVAL) {
            return;
        }
        #else
        if (!localtime_s(&cur_time, &cur_localtime)) {
            return;
        }
        #endif
        char buf[16];
        if (level >= min_level) {
            buf[strftime(buf, sizeof(buf), "%H:%M:%S", &cur_localtime)] = '\0';
            fmt::print("{} {}{:5s}\x1b[0m \x1b[90m{}:{}:\x1b[0m {}\n",
                       buf, level_colors[level], level_strings[level],
                       filename, line,
                       str);
            fflush(stdout);
        }
        if (file) {
            char buf_big[64];
            buf[strftime(buf_big, sizeof(buf_big), "%Y-%m-%d %H:%M:%S", &cur_localtime)] = '\0';
            fmt::print(file, "{} {:5s} {}:{}: {}\n", buf_big, level_strings[level], filename, line, str);
            fflush(file);
        }
    }

};

extern Logger g_logger;

#define log_helper_var(mode, fmtstr, ...) g_logger.log(mode, __FILE__, __LINE__, fmtstr "%s", __VA_ARGS__)
#define log_helper(mode, ...) g_logger.log(mode, __FILE__, __LINE__, __VA_ARGS__, "\n")

#define log_trace(...) log_helper(LOG_TRACE, __VA_ARGS__);
#define log_debug(...) log_helper(LOG_DEBUG, __VA_ARGS__);
#define log_info(...)  log_helper(LOG_INFO, __VA_ARGS__); 
#define log_warn(...)  log_helper(LOG_WARN, __VA_ARGS__);

#ifndef NDEBUG
#define log_error(...) log_helper(LOG_ERROR, __VA_ARGS__); __debugbreak()
#define log_fatal(...) log_helper(LOG_FATAL, __VA_ARGS__); __debugbreak()
#else
#define log_error(...) log_helper(LOG_ERROR, __VA_ARGS__);
#define log_fatal(...) log_helper(LOG_FATAL, __VA_ARGS__);
#endif

#define log_assert(cond, ...) if (!(cond)) { log_helper(LOG_ERROR, __VA_ARGS__); __debugbreak(); }

extern void log_init(const char* filename);
extern void log_release();
extern void log_set_minimum_level(int min_level);
