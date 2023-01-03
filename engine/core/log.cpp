#include "log.h"

Logger g_logger;

void log_init(const char *filename) { g_logger.init(filename); }
void log_release() { g_logger.release(); }
void log_set_minimum_level(int min_level) { g_logger.set_minimum_level(min_level); }
