#include "config.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "path.h"
#include "Logger/logger.h"
#include "Utils/string.h"


std::string load_param_from_file(const std::filesystem::path& file_path, const std::string& default_param_value, const std::string& param_name)
{
    std::string param_value;
    auto abs_file_path = get_mod_base_path() / file_path;
    
    if (std::ifstream(abs_file_path).good())
    {
        log_debug(L"Loading {} from file: '{}'", utf8_to_wstring(param_name), abs_file_path.wstring());
        std::ifstream param_file(file_path);
        param_value = std::string((std::istreambuf_iterator(param_file)), std::istreambuf_iterator<char>());
        param_value = trim(param_value);
    }
    else
    {
        log_info(L"File '{}' not found, using defaults", abs_file_path.wstring());
        log_info(L"Point = ({}, {})", 10, 12);
        param_value = default_param_value;
    }

    log_debug("Loaded {}='{}'", param_name, param_value);
    
    return param_value;
}
