#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include "logger.h"

#include <codecvt>
#include <cstdint>
#include <fstream>

#include "Config/logger-config.h"


std::mutex& get_log_mutex()
{
    static std::mutex s_log_mutex;
    return s_log_mutex;
}


std::optional<std::ostream>& get_log_file_stream()
{
    static std::optional<std::ostream> s_log_file_stream = std::optional<std::ostream>();
    return s_log_file_stream;
}

std::optional<std::wostream>& get_log_file_wstream()
{
    static std::optional<std::wostream> s_log_file_stream = std::optional<std::wostream>();
    return s_log_file_stream;
}


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


void init_logging(std::filebuf& log_file_buffer)
{
    static std::wbuffer_convert<std::codecvt_utf8<wchar_t>> log_file_conv(&log_file_buffer);

    auto& log_file_stream = get_log_file_stream();
    auto& log_file_wstream = get_log_file_wstream();

    log_file_stream.emplace(&log_file_buffer);
    log_file_wstream.emplace(&log_file_conv);
}
