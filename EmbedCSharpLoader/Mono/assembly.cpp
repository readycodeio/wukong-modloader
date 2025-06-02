#include "assembly.h"

#include <filesystem>

#include "Mono/assembly-internals.h"

#include <optional>
#include <windows.h>

#include "Mono/image.h"
#include "Logger/logger.h"
#include "Memory/scanner.h"


typedef void* (*mono_assembly_request_open_t)(const char* filename, const MonoAssemblyOpenRequest* open_req, MonoImageOpenStatus* status);


static std::optional<void***> g_bundles_ptr;
static std::optional<void*> g_mono_assembly_request_open_ptr;


void*** get_bundles_ptr()
{
    if (!g_bundles_ptr.has_value())
    {
        uint64_t bundle_user_ptr = signature(
            "4c 8b 1d ? ? ? ? "
            "48 8b d8 "
            "4d 8b cb "
            "45 33 d2 "
            "49 8b 01 "
            "48 85 c0 "
            "74 ? "
            "48 8b 00 "
            "4c 8b c3 "
            "4c 2b c0 "
            "0f 1f 84 00 00 00 00 00"
        );
        
        if (bundle_user_ptr == 0)
        {
            log_error_missing_ptr("bundle_user_ptr");
            g_bundles_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("bundle_user_ptr", bundle_user_ptr);
        
        uint32_t bundles_offset = *reinterpret_cast<uint32_t*>(bundle_user_ptr + 3);
        g_bundles_ptr = reinterpret_cast<void***>(bundle_user_ptr + 7 + bundles_offset);

        log_debug_ptr("bundles_ptr", bundles_offset);
    }

    return g_bundles_ptr.value();
}


bool mono_register_bundled_assemblies(MonoBundledAssembly **assemblies)
{
    auto bundles_ptr = get_bundles_ptr();

    if (!bundles_ptr)
    {
        log_error("Cannot register bundled assemblies due to missing bundles_ptr.");
        return false;
    }

    *bundles_ptr = reinterpret_cast<void**>(assemblies);
    return true;
}


void* get_mono_assembly_request_open_ptr()
{
    if (!g_mono_assembly_request_open_ptr.has_value())
    {
        auto mono_assembly_request_open = signature(
            "40 55 41 56 41 57 48 8d ac 24 40 ff ff ff 48 81 ec c0 01 00 00 48 8b 05 ? ? ? ? 48 33 c4 48 89 85 90 00 00 00 48 89 4d 90"
        );
        
        if (!mono_assembly_request_open)
        {
            log_error_missing_ptr("mono_assembly_request_open_ptr");
            g_mono_assembly_request_open_ptr = nullptr;
            return nullptr;
        }

        g_mono_assembly_request_open_ptr = reinterpret_cast<void*>(mono_assembly_request_open);
    }

    return g_mono_assembly_request_open_ptr.value();
}


void* mono_assembly_request_open(const std::filesystem::path& filename)
{
    auto func = reinterpret_cast<mono_assembly_request_open_t>(get_mono_assembly_request_open_ptr());
    if (!func)
    {
        log_error("mono_assembly_request_open function pointer is null.");
        return nullptr;
    }
    
    wchar_t full_filename[MAX_PATH];
    GetFullPathNameW(filename.c_str(), MAX_PATH, full_filename, nullptr);
    char full_filenameA[MAX_PATH];
    // convert to utf-8 to support Chinese path
    WideCharToMultiByte(CP_UTF8, 0, full_filename, MAX_PATH, full_filenameA, MAX_PATH, nullptr, nullptr);
    log_info("Loading CSharpManager from: {}", full_filenameA);
    
    MonoAssemblyOpenRequest open_request{};
    auto status = MONO_IMAGE_OK;
    void* assembly = func(full_filenameA, &open_request, &status);

    if (assembly == nullptr)
    {
        log_error("mono_assembly_request_open status failed.");;
        return nullptr;
    }
    
    if (status != MONO_IMAGE_OK)
    {
        log_error("mono_assembly_request_open failed.");;
        return nullptr;
    }
    
    return assembly;
}
