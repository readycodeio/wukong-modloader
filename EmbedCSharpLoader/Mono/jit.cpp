#include "jit.h"

#include <optional>

#include "Logger/logger.h"
#include "Memory/patch.h"
#include "Memory/scanner.h"


static std::optional<void*> g_memory_func_ptr;
static std::optional<void*> g_mono_mode_ptr;


void* get_memory_func_ptr()
{
    if (!g_memory_func_ptr.has_value())
    {
        uint64_t memory_function_ptr = signature("83 3D ? ? ? ? 00 0F 84 ? ? ? ? C7 84 24 ? ? 00 00 01 00 00 00");
        
        if (!memory_function_ptr)
        {
            log_error_missing_ptr("memory_function_ptr");
            g_memory_func_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("memory_function_ptr", memory_function_ptr);
        g_memory_func_ptr = reinterpret_cast<void*>(memory_function_ptr);
    }
    
    return g_memory_func_ptr.value();
}


void* get_mono_mode_ptr()
{
    if (!g_mono_mode_ptr.has_value())
    {
        uint64_t mono_mode_ptr = signature("48 8D 0D ? ? ? ? E8 ? ? ? ? 89 44 24 ? 83 7C 24 ? 00");

        if (!mono_mode_ptr)
        {
            log_error_missing_ptr("mono_mode_ptr");
            g_mono_mode_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_mode_ptr", mono_mode_ptr);
        g_mono_mode_ptr = reinterpret_cast<void*>(mono_mode_ptr);
    }

    return g_mono_mode_ptr.value();
}


bool can_enable_jit()
{
    return get_memory_func_ptr() != nullptr  &&  
           get_mono_mode_ptr() != nullptr;
}


bool enable_jit()
{
    auto memory_function_ptr = get_memory_func_ptr();
    auto mono_mode_ptr = get_mono_mode_ptr();

    if (!memory_function_ptr || !mono_mode_ptr)
    {
        log_error("Cannot enable JIT due to missing function pointers");
        return false;
    }

    uint16_t patch = 0xe990; // nop; jmp
    if (!patch_set_data(reinterpret_cast<uint64_t>(memory_function_ptr) + 7, &patch, sizeof(uint16_t)))
        return false;

    if (!patch_data(reinterpret_cast<uint64_t>(mono_mode_ptr) + 7, 5, [mono_mode_ptr]
    {
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uint64_t>(mono_mode_ptr) + 7) = 0xB8;
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint64_t>(mono_mode_ptr) + 8) = 1;
    }))
        return false;

    return true;
}
