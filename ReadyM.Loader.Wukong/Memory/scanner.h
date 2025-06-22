#pragma once
#include <cstdint>
#include <functional>
#include <string>


uint64_t signature(const char* module_name, const std::string& sig);
uint64_t signature(const std::string& sig);
uint64_t signature(const char* module_name, const std::string& sig, const std::function<bool(const uint8_t*)>& fn);
uint64_t signature(const std::string& sig, const std::function<bool(const uint8_t*)>& fn);
