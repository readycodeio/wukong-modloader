#include "console.h"

#include <cstdio>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <windows.h>


void create_console()
{
    AllocConsole();
    
    FILE* fp = nullptr;

    if (GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONIN$", "r", stdin) == 0)
            setvbuf(stdin, NULL, _IONBF, 0);

    if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONOUT$", "w", stdout) == 0)
            setvbuf(stdout, NULL, _IONBF, 0);

    if (GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONOUT$", "w", stderr) == 0)
            setvbuf(stderr, NULL, _IONBF, 0);

    std::ios::sync_with_stdio(true);

    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();
}
