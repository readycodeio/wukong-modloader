#include "console.h"

#include <cstdio>
#include <windows.h>


void create_console()
{
    AllocConsole();
    FILE* fdummy;
    freopen_s(&fdummy, "CONIN$", "r", stdin);
    freopen_s(&fdummy, "CONOUT$", "w", stdout);
    freopen_s(&fdummy, "CONOUT$", "w", stderr);
}
