#include "assembly.h"

#include <filesystem>
#include <fstream>

#include "Mono/assembly-internals.h"

#include <optional>
#include <algorithm>
#include <windows.h>

#include "Config/path.h"
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


MonoBundledAssembly** get_mono_register_bundled_assemblies()
{
    auto bundles_ptr = get_bundles_ptr();
    if (!bundles_ptr)
    {
        log_error("Cannot get mono_register_bundled_assemblies due to missing bundles_ptr.");
        return nullptr;
    }

    return reinterpret_cast<MonoBundledAssembly**>(*bundles_ptr);
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


bool load_assembly_bundles(const std::filesystem::path& dir)
{
    auto full_dir = get_mod_base_path() / dir;

    auto old_bundles = get_mono_register_bundled_assemblies();
    std::vector<MonoBundledAssembly*> new_bundles_arr;

    std::vector<int> used_indices = {};
    
    std::error_code ec;
    const std::filesystem::directory_iterator full_dir_end;
    for (auto it = std::filesystem::directory_iterator(full_dir, ec); !ec && it != full_dir_end; it.increment(ec))
    {
        if (it->is_regular_file(ec) && it->path().extension() == ".dll")
        {
            std::ifstream dll_file(it->path(), std::ios::in | std::ios::binary | std::ios::ate);
            
            if (dll_file)
            {
                log_debug(L"Loading DLL override: {}", it->path().wstring());
                auto size = dll_file.tellg();
                log_debug("File size is: {} bytes", static_cast<size_t>(size));
                dll_file.seekg(0, std::ios::beg);
                auto buffer = new char[size];
                dll_file.read(buffer, size);

                auto assembly_name = it->path().stem().string() + ".dll";

                auto found_old = false;
                for (int i = 0; old_bundles[i]; ++i)
                {
                    auto old_bundle = old_bundles[i];
                    if (assembly_name == old_bundle->name)
                    {
                        log_debug("Replacing existing bundle: {}", assembly_name);
                        auto new_bundle = glib_new0<MonoBundledAssembly>();
                        new_bundle->name = old_bundle->name;
                        new_bundle->data = reinterpret_cast<const unsigned char*>(buffer);
                        new_bundle->size = size;
                        new_bundles_arr.push_back(new_bundle);
                        used_indices.push_back(i);
                        found_old = true;
                        break;
                    }
                }

                if (!found_old)
                {
                    auto assembly_name_cstr = new char[assembly_name.size() + 1];
                    strcpy_s(assembly_name_cstr, assembly_name.size() + 1, assembly_name.c_str());
                    
                    log_debug("Appending new bundle: {}", assembly_name);
                    auto new_bundle = glib_new0<MonoBundledAssembly>();
                    new_bundle->data = reinterpret_cast<const unsigned char*>(buffer);
                    new_bundle->size = size;
                    new_bundle->name = assembly_name_cstr;
                    new_bundles_arr.push_back(new_bundle);
                }
            }
        }
    }

    for (int i = 0; old_bundles[i]; ++i)
    {
        if (std::ranges::find(used_indices, i) != used_indices.end())
            continue;
        auto old_bundle = old_bundles[i];
        new_bundles_arr.push_back(old_bundle);
    }

    auto new_bundles = new MonoBundledAssembly*[new_bundles_arr.size() + 1];
    for (size_t i = 0; i < new_bundles_arr.size(); ++i)
    {
        new_bundles[i] = new_bundles_arr[i];
    }
    new_bundles[new_bundles_arr.size()] = nullptr;

    return mono_register_bundled_assemblies(new_bundles);
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

    auto full_filename = get_mod_base_path() / filename;
    
    char full_filenameA[MAX_PATH];
    // convert to utf-8 to support Chinese path
    WideCharToMultiByte(CP_UTF8, 0, full_filename.c_str(), MAX_PATH, full_filenameA, MAX_PATH, nullptr, nullptr);
    log_info(L"Loading CSharpManager from: {}", full_filename.c_str());
    
    MonoAssemblyOpenRequest open_request{};
    auto status = MONO_IMAGE_OK;
    void* assembly = func(full_filenameA, &open_request, &status);

    if (assembly == nullptr)
    {
        log_error("mono_assembly_request_open status failed.");
        return nullptr;
    }
    
    if (status != MONO_IMAGE_OK)
    {
        log_error("mono_assembly_request_open failed.");
        return nullptr;
    }
    
    return assembly;
}
