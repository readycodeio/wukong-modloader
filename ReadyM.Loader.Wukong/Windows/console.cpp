#include "console.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <locale>
#include <io.h>
#include <windows.h>
#include <fcntl.h>

#include "Config/logger-config.h"
#include "Logger/logger.h"


HANDLE& get_log_file_handle()
{
    static HANDLE s_log_file_handle = INVALID_HANDLE_VALUE;

    return s_log_file_handle;
}


bool create_console()
{
    AllocConsole();

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    FILE* fp = nullptr;

    auto result = true;
    
    if (GetStdHandle(STD_INPUT_HANDLE) == INVALID_HANDLE_VALUE ||
        freopen_s(&fp, "CONIN$", "r", stdin) != 0 ||
        setvbuf(stdin, nullptr, _IONBF, 0) != 0)
        result = false;

    if (GetStdHandle(STD_OUTPUT_HANDLE) == INVALID_HANDLE_VALUE ||
        freopen_s(&fp, "CONOUT$", "w", stdout) != 0 ||
        setvbuf(stdout, nullptr, _IONBF, 0) != 0)
        result = false;

    if (GetStdHandle(STD_ERROR_HANDLE) == INVALID_HANDLE_VALUE ||
        freopen_s(&fp, "CONOUT$", "w", stderr) != 0 ||
        setvbuf(stderr, nullptr, _IONBF, 0) != 0)
        result = false;

    std::ios::sync_with_stdio(true);

    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();
    
    return result;
}


bool init_console_logging()
{
    auto log_file_path = get_log_file_path();
    auto& log_file_handle = get_log_file_handle();
    
    log_file_handle = CreateFileW(
        log_file_path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr
    );

    if (log_file_handle == INVALID_HANDLE_VALUE)
        return false;
    
    auto file_descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(log_file_handle), 0);
    
    if (file_descriptor == -1)
        return false;
    
    auto file = _fdopen(file_descriptor, "w");

    if (setvbuf(file, nullptr, _IONBF, 0) != 0)
        return false;
    
    static std::filebuf log_file_buffer(file);
    init_logging(log_file_buffer);
    return true;
}
