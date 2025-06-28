#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include "logger.h"

#include <codecvt>
#include <cstdint>
#include <fstream>

#include "Config/logger-config.h"


std::unique_ptr<std::ostream> g_log_file_stream;
std::unique_ptr<std::wostream> g_log_file_wstream;
int g_log_file_stream_fd = -1;


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

    g_log_file_stream = std::make_unique<std::ostream>(&log_file_buffer);
    g_log_file_wstream = std::make_unique<std::wostream>(&log_file_conv);
}
