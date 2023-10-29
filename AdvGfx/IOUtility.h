#pragma once

std::string get_current_directory_path();
std::string read_file_to_string(const std::string& path);
std::string get_file_name_from_path_string(const std::string& path);

// Taken from: https://stackoverflow.com/questions/65195841/hash-function-to-switch-on-a-string
inline constexpr u64 hashstr(const char* s, size_t index = 0) {
    return s + index == nullptr || s[index] == '\0' ? 55 : hashstr(s, index + 1) * 33 + (unsigned char)(s[index]);
}
