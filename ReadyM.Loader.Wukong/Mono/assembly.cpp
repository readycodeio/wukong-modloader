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
#include "Memory/common.h"
#include "Memory/patch.h"
#include "Memory/scanner.h"


typedef void* (*mono_assembly_request_open_t)(const char* filename, const MonoAssemblyOpenRequest* open_req, MonoImageOpenStatus* status);
typedef void* (*mono_assembly_get_image_t)(MonoAssembly* assembly);


static std::optional<void***> g_bundles_ptr;
static std::optional<void*> g_mono_assembly_request_open_ptr;
static std::optional<void*> g_mono_register_bundled_assemblies_ptr;
static std::optional<void*> g_mono_assembly_get_image_ptr;


void*** get_bundles_ptr()
{
    if (!g_bundles_ptr.has_value())
    {
        uint64_t bundles_user_func = signature(
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
        
        if (!bundles_user_func)
        {
            log_error_missing_ptr("bundles_user_func");
            g_bundles_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("bundles_user_func", bundles_user_func);
        
        uint32_t bundles_offset = *reinterpret_cast<uint32_t*>(bundles_user_func + 3);
        g_bundles_ptr = reinterpret_cast<void***>(bundles_user_func + 7 + bundles_offset);

        log_debug_ptr("bundles", bundles_offset);
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


void* get_mono_register_bundled_assemblies_ptr()
{
    if (!g_mono_register_bundled_assemblies_ptr.has_value())
    {
        auto bundles_ptr = reinterpret_cast<uint64_t>(get_bundles_ptr());
        if (!bundles_ptr)
        {
            log_error("Cannot get mono_register_bundled_assemblies_ptr due to missing bundles_ptr.");
            return nullptr;
        }
    
        uint64_t mono_register_bundled_assemblies = signature(
            "48 89 0d "
            "? ? ? ? "
            "c3",
            [&](const uint8_t* ptr) {
                int32_t bundles_offset = static_cast<int32_t>(bundles_ptr - (reinterpret_cast<uint64_t>(ptr) + 7));
                int32_t candidate_offset = *reinterpret_cast<const int32_t*>(ptr + 3);
                return candidate_offset == bundles_offset;
            }
        );

        if (!mono_register_bundled_assemblies)
        {
            log_error_missing_ptr("mono_register_bundled_assemblies");
            g_mono_register_bundled_assemblies_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_register_bundled_assemblies", mono_register_bundled_assemblies);
        g_mono_register_bundled_assemblies_ptr = reinterpret_cast<void*>(mono_register_bundled_assemblies);
    }

    return g_mono_register_bundled_assemblies_ptr.value();
}


extern "C" void*** g_bundles_ptr_exported;
extern "C" void(*g_bundled_assemblies_callback)();
extern "C" void bundled_assemblies_callback_trampoline();


bool intercept_register_bundled_assemblies(void(*callback)())
{
    auto mono_register_bundled_assemblies_ptr = reinterpret_cast<uint64_t>(get_mono_register_bundled_assemblies_ptr());
    if (!mono_register_bundled_assemblies_ptr)
    {
        log_error("Cannot intercept mono_register_bundled_assemblies due to missing ptr offsets.");
        return false;
    }

    uint8_t instr_patch[] = {
        // MOV RDX
        0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // JMP RDX
        0x48, 0xFF, 0xE2
    };
    // Set the callback address in the patch
    *reinterpret_cast<void**>(instr_patch + 2) = &bundled_assemblies_callback_trampoline;

    g_bundles_ptr_exported = get_bundles_ptr();
    g_bundled_assemblies_callback = callback;

    auto patch_instr_ptr = mono_register_bundled_assemblies_ptr + 0;
    auto patch_instr_rva = patch_instr_ptr - g_exe_base_address;
    if (!patch_set_data(g_main_module_name, patch_instr_rva, instr_patch, sizeof(instr_patch)))
    {
        log_error("Failed to patch instruction at {:x}", patch_instr_rva);
        return false;
    }

    return true;
}


bool load_assembly_bundles(std::vector<std::filesystem::path> dirs)
{
    auto old_bundles = get_mono_register_bundled_assemblies();
    std::vector<MonoBundledAssembly*> new_bundles_arr;

    std::vector<int> used_indices = {};

    auto process_assembly = [&old_bundles, &new_bundles_arr, &used_indices](std::filesystem::path path)
    {
        log_debug(L"Processing: {}", path.wstring());
        std::ifstream dll_file(path, std::ios::in | std::ios::binary | std::ios::ate);
        
        if (dll_file)
        {
            log_debug(L"Loading DLL override: {}", path.wstring());
            auto file_size = dll_file.tellg();
            log_debug("File size is: {} bytes", static_cast<size_t>(file_size));
            dll_file.seekg(0, std::ios::beg);
            auto buffer = new char[file_size];
            dll_file.read(buffer, file_size);

            auto assembly_name = path.stem().string() + ".dll";

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
                    new_bundle->size = static_cast<unsigned int>(file_size);
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
                new_bundle->size = static_cast<unsigned int>(file_size);
                new_bundle->name = assembly_name_cstr;
                new_bundles_arr.push_back(new_bundle);
            }
        }
    };
    
    for (auto& dir : dirs)
    {
        auto full_dir = get_mod_base_path() / dir;
        log_debug(L"Full dir: {}", full_dir.wstring());
    
        if (!std::filesystem::exists(full_dir))
        {
            log_debug(L"Directory does not exist: {}", full_dir.wstring());
            continue;
        }

        if (std::filesystem::is_directory(full_dir))
        {
            std::error_code ec;
            const std::filesystem::directory_iterator full_dir_end;
            for (auto it = std::filesystem::directory_iterator(full_dir, ec); !ec && it != full_dir_end; it.increment(ec))
            {
                if (it->is_regular_file(ec) && it->path().extension() == ".dll")
                {
                    process_assembly(it->path());
                }
            }
        }
        else
        {
            if (std::filesystem::is_regular_file(full_dir) && full_dir.extension() == ".dll")
            {
                process_assembly(full_dir);
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
            log_error_missing_ptr("mono_assembly_request_open");
            g_mono_assembly_request_open_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_assembly_request_open", mono_assembly_request_open);
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


void* get_mono_assembly_get_image_ptr()
{
    if (!g_mono_assembly_get_image_ptr.has_value())
    {
        auto mono_assembly_get_image = signature(
            "40 53 "
            "48 83 ec 30 "
            "8b 1d ? ? ? ? "
            "81 c3 ? ? ? ? "
            "48 8d 04 1c "
            "48 89 cb "
            "48 89 44 24 20 "
            "48 8d 4c 24 20 "
            "48 8d 05 ? ? ? ? "
            "48 89 44 24 28"
        );
        
        if (!mono_assembly_get_image)
        {
            log_error_missing_ptr("mono_assembly_get_image");
            g_mono_assembly_get_image_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_assembly_get_image", mono_assembly_get_image);
        g_mono_assembly_get_image_ptr = reinterpret_cast<void*>(mono_assembly_get_image);
    }

    return g_mono_assembly_get_image_ptr.value();
}


void* mono_assembly_get_image(MonoAssembly* assembly)
{
    auto func = reinterpret_cast<mono_assembly_get_image_t>(get_mono_assembly_get_image_ptr());
    if (!func)
    {
        log_error("mono_assembly_get_image function pointer is null.");
        return nullptr;
    }

    return func(assembly);
}
