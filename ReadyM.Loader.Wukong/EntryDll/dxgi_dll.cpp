#pragma unmanaged
#include "dxgi_dll.h"
#include <windows.h>
#include <dxgi.h>

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
void static setup_dll_func(T*& func_ptr, const char* func_name)
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


// 1) New API functions (present in newer API, not in old Windows 8 export set)

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI ApplyCompatResolutionQuirking(int64_t p1, void* p2, void* p3, int64_t p4) {
    D(ApplyCompatResolutionQuirking, p1, p2, p3, p4)
}

// dxgi1_3.h
extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    D(CreateDXGIFactory2, Flags, riid, ppFactory)
}

// dxgi1_6.h
extern "C" HRESULT WINAPI DXGIDeclareAdapterRemovalSupport() {
    D(DXGIDeclareAdapterRemovalSupport)
}

// pix3.h
extern "C" HRESULT WINAPI PIXBeginCapture(DWORD captureFlags, const void* captureParameters) {
    D(PIXBeginCapture, captureFlags, captureParameters)
}

// pix3.h
extern "C" HRESULT WINAPI PIXEndCapture(BOOL discard) {
    D(PIXEndCapture, discard)
}

// pix3.h
extern "C" DWORD WINAPI PIXGetCaptureState() {
    D(PIXGetCaptureState)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" void WINAPI UpdateHMDEmulationStatus(char p1) {
    D(UpdateHMDEmulationStatus, p1)
}

// 2) Old API functions (Windows 8 exports that no longer appear in the newer API export set)

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTCloseAdapter(void* pData) {
    D(D3DKMTCloseAdapter, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTCreateAllocation(void* pData) {
    D(D3DKMTCreateAllocation, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTCreateContext(void* pData) {
    D(D3DKMTCreateContext, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTCreateDevice(void* pData) {
    D(D3DKMTCreateDevice, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTCreateSynchronizationObject(void* pData) {
    D(D3DKMTCreateSynchronizationObject, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTDestroyAllocation(void* pData) {
    D(D3DKMTDestroyAllocation, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTDestroyContext(void* pData) {
    D(D3DKMTDestroyContext, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTDestroyDevice(void* pData) {
    D(D3DKMTDestroyDevice, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTDestroySynchronizationObject(void* pData) {
    D(D3DKMTDestroySynchronizationObject, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTEscape(void* pData) {
    D(D3DKMTEscape, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTGetContextSchedulingPriority(void* pData) {
    D(D3DKMTGetContextSchedulingPriority, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTGetDeviceState(void* pData) {
    D(D3DKMTGetDeviceState, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTGetDisplayModeList(void* pData) {
    D(D3DKMTGetDisplayModeList, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTGetMultisampleMethodList(void* pData) {
    D(D3DKMTGetMultisampleMethodList, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTGetRuntimeData(void* pData) {
    D(D3DKMTGetRuntimeData, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTGetSharedPrimaryHandle(void* pData) {
    D(D3DKMTGetSharedPrimaryHandle, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTLock(void* pData) {
    D(D3DKMTLock, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTOpenAdapterFromHdc(void* pData) {
    D(D3DKMTOpenAdapterFromHdc, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTOpenResource(void* pData) {
    D(D3DKMTOpenResource, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTPresent(void* pData) {
    D(D3DKMTPresent, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTQueryAdapterInfo(void* pData) {
    D(D3DKMTQueryAdapterInfo, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTQueryAllocationResidency(void* pData) {
    D(D3DKMTQueryAllocationResidency, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTQueryResourceInfo(void* pData) {
    D(D3DKMTQueryResourceInfo, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTRender(void* pData) {
    D(D3DKMTRender, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTSetAllocationPriority(void* pData) {
    D(D3DKMTSetAllocationPriority, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTSetContextSchedulingPriority(void* pData) {
    D(D3DKMTSetContextSchedulingPriority, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTSetDisplayMode(void* pData) {
    D(D3DKMTSetDisplayMode, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTSetDisplayPrivateDriverFormat(void* pData) {
    D(D3DKMTSetDisplayPrivateDriverFormat, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTSetGammaRamp(void* pData) {
    D(D3DKMTSetGammaRamp, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTSetVidPnSourceOwner(void* pData) {
    D(D3DKMTSetVidPnSourceOwner, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTSignalSynchronizationObject(void* pData) {
    D(D3DKMTSignalSynchronizationObject, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTUnlock(void* pData) {
    D(D3DKMTUnlock, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTWaitForSynchronizationObject(void* pData) {
    D(D3DKMTWaitForSynchronizationObject, pData)
}

// d3dkmthk.h
extern "C" NTSTATUS APIENTRY D3DKMTWaitForVerticalBlankEvent(void* pData) {
    D(D3DKMTWaitForVerticalBlankEvent, pData)
}

// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_openadapter
extern "C" HRESULT WINAPI OpenAdapter10(void* pOpenData) {
    D(OpenAdapter10, pOpenData)
}

// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_openadapter
extern "C" HRESULT WINAPI OpenAdapter10_2(void* pOpenData) {
    D(OpenAdapter10_2, pOpenData)
}

// https://github.com/PeterTh/gedosato/blob/master/reference/dxgi.dll.exports
extern "C" void WINAPI DXGID3D10ETWRundown() {
    D(DXGID3D10ETWRundown)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" void WINAPI DXGIRevertToSxS() {
    D(DXGIRevertToSxS)
}

// 3) Remaining functions (still appear, but not clearly “new-only” vs “old-only”)

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" void* WINAPI CompatString(int64_t p1) {
    D(CompatString, p1)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" void* WINAPI CompatValue(int64_t p1, int64_t p2) {
    D(CompatValue, p1, p2)
}

// dxgi.h
extern "C" HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory) {
    D(CreateDXGIFactory, riid, ppFactory)
}

// dxgi.h
extern "C" HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    D(CreateDXGIFactory1, riid, ppFactory)
}

// dxgi1_3.h
extern "C" int64_t WINAPI DXGID3D10CreateDevice(void* p1, void* p2) {
    D(DXGID3D10CreateDevice, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI DXGID3D10CreateLayeredDevice(void* p1, void* p2) {
    D(DXGID3D10CreateLayeredDevice, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI DXGID3D10GetLayeredDeviceSize(void* p1, int64_t p2) {
    D(DXGID3D10GetLayeredDeviceSize, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI DXGID3D10RegisterLayers(void* p1, int64_t p2) {
    D(DXGID3D10RegisterLayers, p1, p2)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" int64_t WINAPI DXGIDumpJournal(void* p1) {
    D(DXGIDumpJournal, p1)
}

// dxgi1_3.h
extern "C" HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** pDebug) {
    D(DXGIGetDebugInterface1, Flags, riid, pDebug)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" HRESULT WINAPI DXGIReportAdapterConfiguration(void* p1) {
    D(DXGIReportAdapterConfiguration, p1)
}

// https://strontic.github.io/xcyclopedia/library/dxgi.dll-CE36B98F477E09A567CC2905DC454873.html
extern "C" void WINAPI SetAppCompatStringPointer(void* p1, size_t p2) {
    D(SetAppCompatStringPointer, p1, p2)
}

// 4) Remaining undocumented

// Undocumented
extern "C" void WINAPI DXGIDisableVBlankVirtualization() { 
    D(DXGIDisableVBlankVirtualization)
}