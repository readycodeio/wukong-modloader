#include "logger.h"

#include <cstdint>

void log_debug_ptr(const std::string& name, void* ptr)
{
    log_debug_ptr(name, reinterpret_cast<uint64_t>(ptr));
}

void log_debug_ptr(const std::string& name, uint64_t ptr)
{
    log_debug("{}: 0x{:x}", name, ptr);
}

void log_error_missing_ptr(const std::string& name)
{
    log_error("{} is missing", name);
}
