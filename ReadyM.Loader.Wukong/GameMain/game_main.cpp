#include "game_main.h"

#include <optional>

#include "Memory/common.h"
#include "Memory/patch.h"
#include "Memory/scanner.h"


// NOTE: This has to be a global. in-function static variables are protected by mutexes that cannot be safely used inside
// DllMain as they may depend on Windows APIs that are not permitted inside DllMain
static std::optional<void*> g_b1_main_ptr;


void* get_b1_main_ptr()
{
    if (!g_b1_main_ptr.has_value())
    {
        auto main_b1 = signature(
            "48 8d a4 24 68 ff ff ff "
            "48 89 84 24 20 00 00 00 "
            "48 89 8c 24 28 00 00 00 "
            "48 89 94 24 30 00 00 00 " 
            "48 89 9c 24 38 00 00 00 "
            "48 89 ac 24 40 00 00 00 "
            "48 89 b4 24 48 00 00 00 "
            "48 89 bc 24 50 00 00 00 "
            "4c 89 84 24 58 00 00 00 "
            "4c 89 8c 24 60 00 00 00 "
            "4c 89 94 24 68 00 00 00 "
            "4c 89 9c 24 70 00 00 00 "
            "4c 89 a4 24 78 00 00 00 "
            "4c 89 ac 24 80 00 00 00 "
            "4c 89 b4 24 88 00 00 00 "
            "4c 89 bc 24 90 00 00 00 "
            "48 8d 0d ? ? ? ? "
            "44 8b b1 3c 00 00 00"
        );
        
        if (!main_b1)
        {
            // FIXME: There cannot be any logging here
            // log_error_missing_ptr("b1_main");    
            g_b1_main_ptr = nullptr;
            return nullptr;
        }

        // FIXME: There cannot be any logging here
        // log_debug_ptr("b1_main", main_b1);

        g_b1_main_ptr = reinterpret_cast<void*>(main_b1);
        
        // FIXME: There cannot be any logging here
        // log_debug_ptr("b1_main", g_b1_main_ptr.value());
    }

    return g_b1_main_ptr.value();
}


extern "C" void(*g_game_main_callback)();
extern "C" uint64_t g_game_main_return;
extern "C" void game_main_trampoline();


bool intercept_b1_main(void(*callback)())
{
    auto b1_main_ptr = get_b1_main_ptr();
    
    if (!b1_main_ptr)
        return false;

    uint8_t instr_patch[] = {
        // push rcx
        0x51,
        // mov rcx, <trampoline>
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // jmp rcx
        0xFF, 0xE1
    };
    // Set the callback address in the patch
    *reinterpret_cast<void**>(instr_patch + 3) = reinterpret_cast<void*>(game_main_trampoline);

    g_game_main_callback = callback;
    g_game_main_return = reinterpret_cast<uint64_t>(b1_main_ptr) + 0x88;

    auto patch_instr_ptr = reinterpret_cast<uint64_t>(b1_main_ptr);
    auto patch_instr_rva = patch_instr_ptr - g_exe_base_address;
    if (!patch_set_data(g_main_module_name, patch_instr_rva, instr_patch, sizeof(instr_patch)))
    {
        // FIXME: There cannot be any logging here
        // log_error("Failed to patch instruction at {:x}", patch_instr_rva);
        return false;
    }

    return true;
}