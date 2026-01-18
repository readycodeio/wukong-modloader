#include "usharp.h"

#include <cstdint>
#include <optional>
#include <windows.h>

#include "Mono/mini-runtime.h"
#include "Mono/mono-debug.h"
#include "Logger/logger.h"
#include "Memory/common.h"
#include "Memory/patch.h"
#include "Memory/scanner.h"


int32_t* get_usharp_use_system_env_var_switch_ptr()
{
    static std::optional<int32_t*> s_usharp_use_system_env_var_switch_ptr;

    if (!s_usharp_use_system_env_var_switch_ptr.has_value())
    {
        // "USharp Enable HookEnvVar" unicode string
        uint64_t usharp_enable_hookenv = signature("55 00 53 00 68 00 61 00 72 00 70 00 20 00 45 00 6E 00 61 00 62 00 6C 00 65 00 20 00 48 00 6F 00 6F 00 6B 00 45 00 6E 00 76 00 56 00 61 00 72 00 00 00");

        if (!usharp_enable_hookenv)
        {
            log_error_missing_ptr("usharp_enable_hookenv");
            s_usharp_use_system_env_var_switch_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("usharp_enable_hookenv", usharp_enable_hookenv);
        
        uint64_t usharp_enable_user_func = signature(
            "48 83 EC 38 "
            "C7 44 24 20 00 00 00 00 "
            "4C 8D 0D ? ? ? ? "
            "4C 8D 05 ? ? ? ? "
            "48 8D 15 ? ? ? ? "
            "48 8D 0D ? ? ? ? "
            "E8 ? ? ? ? "
            "48 8D 0D ? ? ? ? "
            "E8 ? ? ? ? "
            "48 83 C4 38 "
            "C3",
            [&](const uint8_t* ptr) {
                int32_t usharp_enable_hookenv_offset = static_cast<int32_t>(usharp_enable_hookenv - (reinterpret_cast<uint64_t>(ptr) + 19));
                int32_t candidate_offset = *reinterpret_cast<const int32_t*>(ptr + 15);
                return candidate_offset == usharp_enable_hookenv_offset;
            }
        );

        if (!usharp_enable_user_func)
        {
            log_error_missing_ptr("usharp_enable_user_func");
            s_usharp_use_system_env_var_switch_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("usharp_enable_user_func", usharp_enable_user_func);
        
        uint32_t use_system_env_var_switch_offset = *reinterpret_cast<uint32_t*>(usharp_enable_user_func + 22);
        s_usharp_use_system_env_var_switch_ptr = reinterpret_cast<int32_t*>((usharp_enable_user_func + 26) + use_system_env_var_switch_offset);

        log_debug_ptr("usharp_use_system_env_var_switch", s_usharp_use_system_env_var_switch_ptr.value());
    }

    return s_usharp_use_system_env_var_switch_ptr.value();
}


bool usharp_use_system_env_var_switch(bool enable)
{
    auto use_system_env_var_switch_ptr = get_usharp_use_system_env_var_switch_ptr();

    if (!use_system_env_var_switch_ptr)
    {
        log_error("Cannot use system env var switch due to missing ptr offsets");
        return false;
    }
    
    const int32_t patch_data[]
    {
        !enable
    };

    auto use_system_env_var_switch_rva = reinterpret_cast<uint64_t>(use_system_env_var_switch_ptr) - g_exe_base_address;
    return patch_set_data(g_main_module_name, use_system_env_var_switch_rva, &patch_data, sizeof(int32_t));
}


void* get_mono_sbd_env_options_ptr()
{
    static std::optional<void*> s_mono_sbd_env_options_ptr;

    if (!s_mono_sbd_env_options_ptr.has_value())
    {
        auto mini_init_ptr = get_mini_init_ptr();
        
        if (!mini_init_ptr)
        {
            s_mono_sbd_env_options_ptr = nullptr;
            return nullptr;
        }

        uint32_t mono_sbd_env_options_offset = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint64_t>(mini_init_ptr) + 47);
        s_mono_sbd_env_options_ptr = reinterpret_cast<void*>((reinterpret_cast<uint64_t>(mini_init_ptr) + 51) + mono_sbd_env_options_offset);

        log_debug_ptr("mono_sbd_env_options", s_mono_sbd_env_options_ptr.value());
    }

    return s_mono_sbd_env_options_ptr.value();
}


bool set_mono_sbd_env_options(const std::string& debugger_agent_opts)
{
    static std::string s_debugger_agent_opts;

    auto mono_sbd_env_options_ptr = get_mono_sbd_env_options_ptr();

    if (!mono_sbd_env_options_ptr)
    {
        log_error("Cannot set MONO_SDB_ENV_OPTIONS due to missing ptr offsets");
        return false;
    }
    
    auto mono_sdb_env_options_rva = reinterpret_cast<uint64_t>(get_mono_sbd_env_options_ptr()) - g_exe_base_address;

    SetEnvironmentVariableA("MONO_SDB_ENV_OPTIONS", nullptr);
    
    const char* patch_data[1];

    s_debugger_agent_opts = debugger_agent_opts;
    patch_data[0] = s_debugger_agent_opts.c_str();

    return patch_set_data(g_main_module_name, mono_sdb_env_options_rva, &patch_data, sizeof(char*));
}


bool can_enable_debugger()
{
    return get_usharp_use_system_env_var_switch_ptr() != nullptr &&
           get_mono_sbd_env_options_ptr() != nullptr &&
           get_mono_debug_init_ptr() != nullptr;
}


bool init_debugger(const std::string& log_level, const std::string& log_mask, const std::string& debugger_agent_opts)
{
    if (!can_enable_debugger())
    {
        log_error("Debugger CANNOT be enabled due to missing ptr offsets");
        return false;
    }

    log_debug("Debugger can be enabled");
    
    SetEnvironmentVariableA("MONO_SDB_ENV_OPTIONS", nullptr);
    SetEnvironmentVariableA("MONO_LOG_LEVEL", log_level.c_str());
    SetEnvironmentVariableA("MONO_LOG_MASK", log_mask.c_str());

    if (!usharp_use_system_env_var_switch(false))
        return false;
    
    if (!set_mono_sbd_env_options(debugger_agent_opts))
        return false;

    if (!mono_debug_init(MONO_DEBUG_FORMAT_MONO))
        return false;

    return true;
}


extern "C" void(*g_csharp_loader_x_load_runtime_x_callback)();
extern "C" void csharp_loader_x_load_runtime_x_callback_trampoline();


bool intercept_csharp_loader_x_load_runtimes(void(*callback)())
{
    uint64_t csharp_loader_x_load_runtimes_x_epilogue = signature(
        "8b 03 "
        "c1 e8 03 "
        "a8 01 "
        "74 ? "
        "83 4b 04 08 "
        "83 7b 04 00 "
        "0f 95 c0 "
        "48 8b 5c 24 58 "
        "48 83 c4 30 "
        "5f "
        "5e "
        "5d "
        "c3"
    );
    
    if (!csharp_loader_x_load_runtimes_x_epilogue)
    {
        log_error_missing_ptr("csharp_loader_x_load_runtimes_x_epilogue");
        return false;
    }

    log_debug_ptr("csharp_loader_x_load_runtimes_x_epilogue", csharp_loader_x_load_runtimes_x_epilogue);

    uint8_t instr_patch[] = {
        // MOV RCX, <trampoline>
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // JMP RCX
        0xFF, 0xE1
    };
    // Set the callback address in the patch
    *reinterpret_cast<void**>(instr_patch + 2) = reinterpret_cast<void*>(csharp_loader_x_load_runtime_x_callback_trampoline);

    g_csharp_loader_x_load_runtime_x_callback = callback;

    auto patch_instr_ptr= csharp_loader_x_load_runtimes_x_epilogue + 25;
    auto patch_instr_rva = patch_instr_ptr - g_exe_base_address;
    if (!patch_set_data(g_main_module_name, patch_instr_rva, instr_patch, sizeof(instr_patch)))
    {
        log_error("Failed to patch instruction at {:x}", patch_instr_rva);
        return false;
    }

    return true;
}


extern "C" void(*g_csharp_loader_x_load_x_callback)();
extern "C" void csharp_loader_x_load_x_callback_trampoline();


bool intercept_csharp_loader_x_load(void(*callback)())
{
    uint64_t csharp_loader_x_load_x_epilogue = signature(
        "48 8b 8d "
        "d0 04 00 00 "
        "48 33 cc "
        "e8 ? ? ? ? "
        "48 81 c4 e8 05 00 00 "
        "41 5f "
        "41 5e "
        "41 5d "
        "41 5c "
        "5f "
        "5e "
        "5b "
        "5d "
        "c3"
    );
    
    if (!csharp_loader_x_load_x_epilogue)
    {
        log_error_missing_ptr("csharp_loader_x_load_x_epilogue");
        return false;
    }

    log_debug_ptr("csharp_loader_x_load_x_epilogue", csharp_loader_x_load_x_epilogue);

    uint8_t instr_patch[] = {
        // MOV RCX, <callback_trampoline>
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // JMP RCX
        0xFF, 0xE1
    };
    // Set the callback address in the patch
    *reinterpret_cast<void**>(instr_patch + 2) = reinterpret_cast<void*>(csharp_loader_x_load_x_callback_trampoline);

    g_csharp_loader_x_load_x_callback = callback;

    auto patch_instr_ptr= csharp_loader_x_load_x_epilogue + 15;
    auto patch_instr_rva = patch_instr_ptr - g_exe_base_address;
    if (!patch_set_data(g_main_module_name, patch_instr_rva, instr_patch, sizeof(instr_patch)))
    {
        log_error("Failed to patch instruction at {:x}", patch_instr_rva);
        return false;
    }

    return true;
}
