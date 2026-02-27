#include <windows.h>
#include <delayimp.h>

static void ShowMissingDepA(const char* dllNameA) {
    wchar_t dllNameW[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, dllNameA ? dllNameA : "unknown", -1, dllNameW, MAX_PATH);

    wchar_t msg[512];
    wsprintfW(msg,
              L"Missing dependency: %s\n\n"
              L"Please run the game through the ReadyM Launcher to reinstall missing mod files.",
              dllNameW);

    MessageBoxW(nullptr, msg, L"Startup Error", MB_ICONERROR | MB_OK);
}

static FARPROC WINAPI DelayHook(unsigned dliNotify, PDelayLoadInfo pdli) {
    if (dliNotify == dliFailLoadLib && pdli && pdli->szDll) {
        ShowMissingDepA(pdli->szDll);
    }
    return nullptr; // signal failure
}

// Assign the global hook pointer (declared in delayimp.h) to our function.
extern "C" PfnDliHook __pfnDliFailureHook2 = DelayHook;