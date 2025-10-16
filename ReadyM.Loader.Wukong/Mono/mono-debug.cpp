#include "mono-debug.h"

#include <filesystem>
#include <fstream>
#include <optional>

#include "Config/path.h"
#include "Logger/logger.h"
#include "Memory/common.h"
#include "Memory/patch.h"
#include "Memory/scanner.h"
#include "Mono/glib.h"


static std::optional<BundledSymfile**> g_bundled_symfiles_ptr;
static std::optional<void*> g_mono_debug_init_ptr;


BundledSymfile** get_bundled_symfiles_ptr()
{
    if (!g_bundled_symfiles_ptr.has_value())
    {
        uint64_t bundled_symfiles_user_func_ptr = signature(
            "48 8b cb "
            "e8 ? ? ? ? "
            "4c 8b 0d ? ? ? ? "
            "48 8b d8 "
            "4d 85 c9 "
            "74 ? "
            "4c 8b 50 38 "
            "49 8b 49 08 "
            "4d 8b c2 "
            "4c 2b c1 "
            "0f 1f 00 "
            "0f b6 11 "
            "42 0f b6 04 01 "
            "2b d0 "
            "75 ? "
            "48 ff c1 "
            "85 c0 "
            "75 ?"
        );
        
        if (!bundled_symfiles_user_func_ptr)
        {
            log_error_missing_ptr("bundled_symfiles_user_func");
            g_bundled_symfiles_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("bundled_symfiles_user_func", bundled_symfiles_user_func_ptr);
        
        uint32_t bundled_symfiles_offset = *reinterpret_cast<uint32_t*>(bundled_symfiles_user_func_ptr + 11);
        uint64_t instr_base = bundled_symfiles_user_func_ptr + 15;
        g_bundled_symfiles_ptr = reinterpret_cast<BundledSymfile**>(instr_base + bundled_symfiles_offset);
        
        log_debug_ptr("bundled_symfiles", bundled_symfiles_user_func_ptr);
    }

    return g_bundled_symfiles_ptr.value();
}


bool mono_register_symfile_for_assembly(char* assembly_name, const uint8_t *raw_contents, int size)
{
    auto bundled_symfiles_ptr = get_bundled_symfiles_ptr();
    if (!bundled_symfiles_ptr)
    {
        log_error("Cannot register symfile due to missing bundled_symfiles_ptr.");
        return false;
    }
    
    auto bsymfile = glib_new0<BundledSymfile>();
    bsymfile->aname = assembly_name;
    bsymfile->raw_contents = raw_contents;
    bsymfile->size = size;
    bsymfile->next = *g_bundled_symfiles_ptr.value();
    *(g_bundled_symfiles_ptr.value()) = bsymfile;
    return true;
}


void* get_mono_debug_init_ptr()
{
    if (!g_mono_debug_init_ptr.has_value())
    {
        uint64_t mono_debug_init = signature(
            "40 53 "               // push rbx
            "48 83 EC 30 "         // sub rsp, 0x30
            "83 3D ? ? ? ? ? "     // cmp dword ptr [rip+disp32], imm8
            "8B D9 "               // mov ebx, ecx
            "74 ? "                // jz rel8
            "4C 8D 05 ? ? ? ? "    // lea r8, [rip+disp32]
            "BA 67 00 00 00 "      // mov edx, 0x67
            "48 8D 0D ? ? ? ? "    // lea rcx, [rip+disp32]
            "E8 ? ? ? ?"           // call rel32
        );

        if (!mono_debug_init)
        {
            log_error_missing_ptr("debug_init");
            g_mono_debug_init_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("debug_init", mono_debug_init);
        g_mono_debug_init_ptr = reinterpret_cast<void*>(mono_debug_init);
    }

    return g_mono_debug_init_ptr.value();
}



bool mono_debug_init(MonoDebugFormat format)
{
    auto mono_debug_init_ptr = get_mono_debug_init_ptr();
    if (!mono_debug_init_ptr)
    {
        log_error("Cannot initialize debugger due to missing mono_debug_init_ptr.");
        return false;
    }
    
    auto debug_init_func_rva = reinterpret_cast<uint64_t>(mono_debug_init_ptr) - g_exe_base_address;

    if (!patch_call_function(g_main_module_name, debug_init_func_rva, format))
        return false;

    return true;
}


bool can_load_symbols()
{
    return get_bundled_symfiles_ptr() != nullptr &&
           get_mono_debug_init_ptr() != nullptr &&
           get_glib_new0_ptr() != nullptr;
}


bool load_debugger_symbols(const std::vector<std::filesystem::path>& dirs)
{
    if (!can_load_symbols())
    {
        log_error("Cannot load symbols for bundled assemblies due to missing ptr offsets.");
        return false;
    }
    
    log_debug("Loading symbols for bundled assemblies");

    auto process_symbols = [](const std::filesystem::path& path)
    {
        std::ifstream pdb_file(path, std::ios::in | std::ios::binary | std::ios::ate);
            
        if (pdb_file)
        {
            log_debug(L"Loading PDB file: {}", path.wstring());
            auto size = pdb_file.tellg();
            log_debug("File size is: {} bytes", static_cast<size_t>(size));
            pdb_file.seekg(0, std::ios::beg);
            auto buffer = new char[size];
            pdb_file.read(buffer, size);

            auto assembly_name = path.stem().string() + ".dll";
            auto assembly_name_cstr = new char[assembly_name.size() + 1];
            strcpy_s(assembly_name_cstr, assembly_name.size() + 1, assembly_name.c_str());
                
            mono_register_symfile_for_assembly(assembly_name_cstr, reinterpret_cast<const uint8_t*>(buffer), (int)size);
        }
    };

    for (auto& dir : dirs)
    {
        auto full_dir = get_base_dir() / dir;
        
        if (std::filesystem::is_directory(full_dir))
        {
            std::error_code ec;
            const std::filesystem::directory_iterator full_dir_end;
            for (auto it = std::filesystem::directory_iterator(full_dir, ec); !ec && it != full_dir_end; it.increment(ec))
            {
                if (it->is_regular_file(ec) && it->path().extension() == ".pdb")
                {
                    process_symbols(it->path());
                }
            }
        }
        else
        {
            if (std::filesystem::is_regular_file(full_dir) && full_dir.extension() == ".pdb")
            {
                process_symbols(full_dir);
            }
        }
    }
    
    return true;
}
