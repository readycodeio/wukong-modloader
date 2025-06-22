#pragma once
#include <filesystem>
#include <string>


std::string load_param_from_file(const std::filesystem::path& file_path, const std::string& default_param_value, const std::string& param_name);
