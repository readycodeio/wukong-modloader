#pragma unmanaged
#include <windows.h>

#include "Logger/logger.h"
#include "Windows/constants.h"

static HMODULE h_module_dll = nullptr;
static const wchar_t dll_fname[] = L"dxgi";


static bool deinit_hijack_dll();


static bool init_hijack_dll()
{
    deinit_hijack_dll();
    wchar_t system_dir[WIN32_MAX_PATH]{0};
    GetSystemDirectoryW(system_dir, WIN32_MAX_PATH);
    wchar_t full_path_dll_name[WIN32_MAX_PATH]{0};
    if (swprintf_s(full_path_dll_name, WIN32_MAX_PATH, L"%s\\%s.dll", system_dir, dll_fname) == -1)
    {
        log_error(L"Failed to construct full path for DLL: {}", dll_fname);
        return false;
    }
    if ((h_module_dll = LoadLibraryW(full_path_dll_name)) == nullptr)
    {
        log_error(L"Failed to load DLL: {}", dll_fname);
        return false;
    }
    log_debug(L"Successfully loaded DLL: {}", dll_fname);

    if (std::atexit([] {
        deinit_hijack_dll();
    }) != 0)
    {
        log_warn(L"Failed to register DLL deinitialization at exit.");
    }

    return true;
}


static bool ensure_hijack_dll()
{
    if (h_module_dll == nullptr)
    {
        return init_hijack_dll();
    }
    return true;
}


static bool deinit_hijack_dll()
{
    if (h_module_dll != nullptr)
    {
        FreeLibrary(h_module_dll);
        h_module_dll = nullptr;
    }
    return true;
}

template<typename T>
bool static setup_dll_func(T*& func_ptr, const char* func_name)
{
    if (func_ptr != nullptr)
        return true;

    if (!ensure_hijack_dll())
    {
        log_crit("Cannot forward {} because the system dxgi.dll could not be loaded.", func_name);
        return false;
    }

    func_ptr = reinterpret_cast<T*>(GetProcAddress(h_module_dll, func_name));

    if (func_ptr == nullptr)
    {
        // Should not happen for the names below, but export sets do shift slightly between Windows builds.
        // Reporting failure to the caller is always better than calling address zero.
        log_error("The system dxgi.dll does not export {}. Reporting failure to the caller.", func_name);
        return false;
    }

    return true;
}


// IMPORTANT: only re-export names that the real system dxgi.dll actually exports.
//
// Overlays and capture layers (Discord, Steam, RivaTuner, ShadowPlay, OBS, ...) feature-probe dxgi.dll with
// GetProcAddress and take a different code path when a symbol is absent. A proxy that answers those probes
// with a stub makes them call a function the OS does not have. This file used to export 59 names while
// Windows 11 exports 20, so 39 of them resolved to nullptr and were then called unconditionally, which is
// what took the game down for players running an overlay.
//
// The removed set was the Windows 8 era legacy surface: every D3DKMT* thunk (those live in gdi32.dll now),
// OpenAdapter10, OpenAdapter10_2, DXGID3D10ETWRundown and DXGIRevertToSxS. Nothing in a working modern
// process can be importing them by name, because stock dxgi.dll does not provide them either.
//
// To re-check this list after a Windows update:
//   dumpbin /exports C:\Windows\System32\dxgi.dll
//
// `fallback` is what we return if the OS turns out not to export the function after all.
#define D(dll_funcname, fallback, ...)                  \
    static decltype(dll_funcname)* p;                   \
    static bool p_missing;                              \
    if (p_missing || !setup_dll_func(p, #dll_funcname)) \
    {                                                   \
        p_missing = true;                               \
        return fallback;                                \
    }                                                   \
    return p(__VA_ARGS__);                              \
    __pragma(comment(linker, "/EXPORT:" __FUNCTION__))


// Same as D, for functions returning void.
#define D_VOID(dll_funcname, ...)                       \
    static decltype(dll_funcname)* p;                   \
    static bool p_missing;                              \
    if (p_missing || !setup_dll_func(p, #dll_funcname)) \
    {                                                   \
        p_missing = true;                               \
        return;                                         \
    }                                                   \
    p(__VA_ARGS__);                                     \
    __pragma(comment(linker, "/EXPORT:" __FUNCTION__))


// 1) Factory creation. This is what the game itself imports.

// dxgi.h
extern "C" HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory) {
    D(CreateDXGIFactory, E_NOTIMPL, riid, ppFactory)
}

// dxgi.h
extern "C" HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    D(CreateDXGIFactory1, E_NOTIMPL, riid, ppFactory)
}

// dxgi1_3.h
extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    D(CreateDXGIFactory2, E_NOTIMPL, Flags, riid, ppFactory)
}

// dxgi1_3.h
extern "C" HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** pDebug) {
    D(DXGIGetDebugInterface1, E_NOTIMPL, Flags, riid, pDebug)
}

// dxgi1_6.h
extern "C" HRESULT WINAPI DXGIDeclareAdapterRemovalSupport() {
    D(DXGIDeclareAdapterRemovalSupport, E_NOTIMPL)
}


// 2) Device layering. d3d11.dll calls these while building a device.

// dxgi1_3.h
extern "C" int64_t WINAPI DXGID3D10CreateDevice(void* p1, void* p2) {
    D(DXGID3D10CreateDevice, E_NOTIMPL, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI DXGID3D10CreateLayeredDevice(void* p1, void* p2) {
    D(DXGID3D10CreateLayeredDevice, E_NOTIMPL, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
// NOTE: returns a size, not an HRESULT, so the fallback is 0 rather than E_NOTIMPL.
extern "C" int64_t WINAPI DXGID3D10GetLayeredDeviceSize(void* p1, int64_t p2) {
    D(DXGID3D10GetLayeredDeviceSize, 0, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI DXGID3D10RegisterLayers(void* p1, int64_t p2) {
    D(DXGID3D10RegisterLayers, E_NOTIMPL, p1, p2)
}


// 3) PIX capture hooks. Present on Windows 10 1803 and newer.

// pix3.h
extern "C" HRESULT WINAPI PIXBeginCapture(DWORD captureFlags, const void* captureParameters) {
    D(PIXBeginCapture, E_NOTIMPL, captureFlags, captureParameters)
}

// pix3.h
extern "C" HRESULT WINAPI PIXEndCapture(BOOL discard) {
    D(PIXEndCapture, E_NOTIMPL, discard)
}

// pix3.h
extern "C" DWORD WINAPI PIXGetCaptureState() {
    D(PIXGetCaptureState, 0)
}


// 4) App compat shims and diagnostics.

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI ApplyCompatResolutionQuirking(int64_t p1, void* p2, void* p3, int64_t p4) {
    D(ApplyCompatResolutionQuirking, 0, p1, p2, p3, p4)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" void* WINAPI CompatString(int64_t p1) {
    D(CompatString, nullptr, p1)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" void* WINAPI CompatValue(int64_t p1, int64_t p2) {
    D(CompatValue, nullptr, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" HRESULT WINAPI SetAppCompatStringPointer(size_t p1, void* p2) {
    D(SetAppCompatStringPointer, E_NOTIMPL, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI DXGIDumpJournal(void* p1) {
    D(DXGIDumpJournal, E_NOTIMPL, p1)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" HRESULT WINAPI DXGIReportAdapterConfiguration(void* p1) {
    D(DXGIReportAdapterConfiguration, E_NOTIMPL, p1)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" void WINAPI UpdateHMDEmulationStatus(char p1) {
    D_VOID(UpdateHMDEmulationStatus, p1)
}

// Undocumented
extern "C" HRESULT WINAPI DXGIDisableVBlankVirtualization() {
    D(DXGIDisableVBlankVirtualization, E_NOTIMPL)
}
