#pragma once

#include "core/vector.h"

#include <string>

Vector<uint8_t> load_file_to_buffer(std::string_view filename);

std::string load_file_to_string(std::string_view filename);

std::string_view base_path(std::string_view path);
