#include "file.h"

#include "log.h"

#include <physfs.h>

Vector<unsigned char> load_file_to_buffer(std::string_view filename) {
    std::string path = std::string(filename);
    PHYSFS_file* file = PHYSFS_openRead(path.c_str());
    if (file == nullptr) {
        log_error("Failed to load file {}!", filename);
        return {};
    }
    PHYSFS_sint64 file_size = PHYSFS_fileLength(file);
    Vector<unsigned char> buf(file_size);
    PHYSFS_read(file, buf.data(), 1, file_size);
    PHYSFS_close(file);
    return buf;
}

std::string load_file_to_string(std::string_view filename) {
    std::string path = std::string(filename);
    PHYSFS_file* file = PHYSFS_openRead(path.c_str());
    if (file == nullptr) {
        log_error("Failed to load file {}!", filename);
        return {};
    }
    PHYSFS_sint64 file_size = PHYSFS_fileLength(file);
    std::string str;
    str.resize(file_size);
    PHYSFS_read(file, str.data(), 1, file_size);
    PHYSFS_close(file);
    return str;
}

std::string_view base_path(std::string_view path) {
    size_t i = path.rfind('/');
    return path.substr(0, i);
}
