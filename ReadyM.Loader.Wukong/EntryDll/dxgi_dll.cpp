#pragma unmanaged
#include "dxgi_dll.h"

#include <array>
#include <windows.h>

#include "Logger/logger.h"
#include "Windows/constants.h"

static HMODULE h_module_dll = nullptr;
static const wchar_t dll_fname[] = L"dxgi";


bool init_version_dll()
{
    deinit_version_dll();
    wchar_t system_dir[WIN32_MAX_PATH]{0};
    GetSystemDirectoryW(system_dir, WIN32_MAX_PATH);
    wchar_t fullpath_dll_name[WIN32_MAX_PATH]{0};
    swprintf_s(fullpath_dll_name, WIN32_MAX_PATH, L"%s\\%s.dll", system_dir, dll_fname);
    if ((h_module_dll = LoadLibraryW(fullpath_dll_name)) == nullptr)
    {
        return false;
    }
    log_debug(L"Successfully loaded DLL: {}", dll_fname);
    return true;
}


bool deinit_version_dll()
{
    if (h_module_dll != nullptr)
    {
        FreeLibrary(h_module_dll);
        h_module_dll = nullptr;
    }
    return true;
}

template<typename T>
void setup_dll_func(T*& func_ptr, const char* func_name)
{
    if (func_ptr != nullptr)
        return;
    
    func_ptr = reinterpret_cast<T*>(GetProcAddress(h_module_dll, func_name));
}

#define D(dll_funcname, ...)            \
    static decltype(dll_funcname)* p;   \
    setup_dll_func(p, #dll_funcname);   \
    return p(__VA_ARGS__);              \
    __pragma(comment(linker, "/EXPORT:" __FUNCTION__))

// --- DXGI.DLL EXPORTS ---

extern "C" HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory) {
    D(CreateDXGIFactory, riid, ppFactory);
}

extern "C" HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory) {
    D(CreateDXGIFactory1, riid, ppFactory);
}

extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory) {
    D(CreateDXGIFactory2, Flags, riid, ppFactory);
}

extern "C" HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void **pDebug) {
    D(DXGIGetDebugInterface1, Flags, riid, pDebug);
}

extern "C" HRESULT WINAPI DXGIDeclareAdapterRemovalSupport() {
    D(DXGIDeclareAdapterRemovalSupport);
}

// --- Potentially undocumented or legacy functions ---
// Note: Signatures for these are based on available information but may be incomplete.
// Your macro approach is generally safe as it forwards arguments as-is.

extern "C" void WINAPI ApplyCompatResolutionQuirking() { 
    D(ApplyCompatResolutionQuirking); 
}

extern "C" void WINAPI CompatString() { 
    D(CompatString); 
}

extern "C" void WINAPI CompatValue() { 
    D(CompatValue); 
}

extern "C" void WINAPI DXGID3D10CreateDevice() { 
    D(DXGID3D10CreateDevice); 
}

extern "C" void WINAPI DXGID3D10CreateLayeredDevice() { 
    D(DXGID3D10CreateLayeredDevice); 
}

extern "C" void WINAPI DXGID3D10GetLayeredDeviceSize() { 
    D(DXGID3D10GetLayeredDeviceSize); 
}

extern "C" void WINAPI DXGID3D10RegisterLayers() { 
    D(DXGID3D10RegisterLayers); 
}

extern "C" void WINAPI DXGIDisableVBlankVirtualization() { 
    D(DXGIDisableVBlankVirtualization); 
}

extern "C" void WINAPI DXGIDumpJournal() { 
    D(DXGIDumpJournal); 
}

extern "C" void WINAPI DXGIReportAdapterConfiguration() { 
    D(DXGIReportAdapterConfiguration); 
}

extern "C" void WINAPI PIXBeginCapture() { 
    D(PIXBeginCapture); 
}

extern "C" void WINAPI PIXEndCapture() { 
    D(PIXEndCapture); 
}

extern "C" void WINAPI PIXGetCaptureState() { 
    D(PIXGetCaptureState); 
}

extern "C" void WINAPI SetAppCompatStringPointer() { 
    D(SetAppCompatStringPointer); 
}

extern "C" void WINAPI UpdateHMDEmulationStatus() { 
    D(UpdateHMDEmulationStatus); 
}