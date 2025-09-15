#include "signature_check.h"

#include <cstdint>

#include "Logger/logger.h"
#include "Memory/patch.h"
#include "Memory/scanner.h"


static uintptr_t follow_jump(const uintptr_t address)
{
    auto* instruction_ptr = reinterpret_cast<uint8_t*>(address);

    // Check if the instruction at the address is a relative JMP (opcode 0xE9)
    if (0xE9 == *instruction_ptr)
    {
        // Read the 32-bit signed relative offset that follows the opcode
        const int32_t relative_offset = *reinterpret_cast<int32_t*>(address + 1);

        // The jump is relative to the address of the instruction AFTER the JMP.
        // JMP instruction size is 5 bytes (1 for opcode + 4 for offset).
        return address + relative_offset + 5;
    }

    log_error("Error: Instruction at 0x%llX is not a JMP (0xE9).\n", static_cast<unsigned long long>(address));
    return 0;
}

// Original Lua source: https://www.nexusmods.com/blackmythwukong/mods/133?tab=description
bool patch_pak_signature_check()
{
    const std::string sign_txt = "48 8D 0D ?? ?? ?? ?? E9 ?? ?? ?? ?? CC CC CC CC 48 83 EC 28 E8 ?? ?? ?? ?? 48 89 05 ?? ?? ?? ?? 48 83 C4 28 C3 CC CC CC CC CC CC CC CC CC CC CC 48 8D 0D ?? ?? ?? ?? E9 ?? ?? ?? ?? CC CC CC CC 48 8D 0D ?? ?? ?? ?? E9 ?? ?? ?? ?? CC CC CC CC";
    const uint64_t addr = signature(sign_txt);

    if (!addr)
    {
        log_error_missing_ptr("file_open_log");
        return false;
    }

    const auto fst_jmp = follow_jump(addr + 0x37);

    if (fst_jmp == 0)
    {
        log_error("Failed to follow first jump for file_open_log");
        return false;
    }

    const auto snd_jmp = follow_jump(fst_jmp);

    if (snd_jmp == 0)
    {
        log_error("Failed to follow second jump for file_open_log");
        return false;
    }

    log_debug_ptr("file_open_log", snd_jmp);

    // 3. Define the patch data. 0xC3 is the x86 opcode for the RET instruction.
    constexpr uint8_t patch_data[] = {0xC3};

    // 5. Apply the patch using your existing patching utility.
    if (!patch_set_data(snd_jmp, &patch_data, sizeof(patch_data)))
    {
        log_error("Failed to apply patch for file_open_log");
        return false;
    }

    return true;
}
