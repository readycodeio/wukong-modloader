#include "patch.h"

#include <cstdint>
#include <functional>
#include <windows.h>

#include "Logger/logger.h"


bool patch_set_data(const std::string& module_name, uint64_t rva, const void* patch, int patch_size)
{
    const auto h_module = GetModuleHandleA(module_name.c_str());
    if (!h_module)
    {
        log_error("Failed to get module handle for: {}", module_name);
        return false;
    }

    const auto base_address = reinterpret_cast<BYTE*>(h_module);
    const auto target_address = base_address + rva;

    DWORD old_protect;
    if (!VirtualProtect(target_address, patch_size, PAGE_EXECUTE_READWRITE, &old_protect))
    {
        log_error("Failed to change memory protection at address {}+0x{:x} ({} bytes)", module_name, rva, patch_size);
        return false;
    }

    memcpy(target_address, patch, patch_size);

    if (!VirtualProtect(target_address, patch_size, old_protect, &old_protect))
    {
        log_error("Failed to restore memory protection at address {}+0x{:x} ({} bytes)", module_name, rva, patch_size);
        return false;
    }

    if (!FlushInstructionCache(GetCurrentProcess(), target_address, patch_size))
    {
        log_error("Failed to flush instruction cache for address {}+0x{:x} ({} bytes)", module_name, rva, patch_size);
        return false;
    }

    log_debug("Successfully patched {} bytes at address {}+0x{:x}", patch_size, module_name, rva);
    return true;
}


bool patch_set_data(uint64_t addr, const void* patch, int patch_size)
{
    DWORD old_protect;
    if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), patch_size, PAGE_EXECUTE_READWRITE, &old_protect))
    {
        log_error("Failed to change memory protection at address 0x{:x} ({} bytes)", addr, patch_size);
        return false;
    }

    memcpy(reinterpret_cast<LPVOID>(addr), patch, patch_size);

    if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), patch_size, old_protect, &old_protect))
    {
        log_error("Failed to restore memory protection at address 0x{:x} ({} bytes)", addr, patch_size);
        return false;
    }

    if (!FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPVOID>(addr), patch_size))
    {
        log_error("Failed to flush instruction cache for address 0x{:x} ({} bytes)", addr, patch_size);
        return false;
    }

    log_debug("Successfully patched {} bytes at address 0x{:x}", patch_size, addr);
    return true;
}


bool patch_data(uint64_t addr, int patch_size, std::function<void()> fn)
{
    DWORD old_protect;
    if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), patch_size, PAGE_EXECUTE_READWRITE, &old_protect))
    {
        log_error("Failed to change memory protection at address 0x{:x} ({} bytes)", addr, patch_size);
        return false;
    }

    fn();

    if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), patch_size, old_protect, &old_protect))
    {
        log_error("Failed to restore memory protection at address 0x{:x} ({} bytes)", addr, patch_size);
        return false;
    }

    if (!FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPVOID>(addr), patch_size))
    {
        log_error("Failed to flush instruction cache for address 0x{:x} ({} bytes)", addr, patch_size);
        return false;
    }

    log_debug("Successfully patched {} bytes at address 0x{:x}", patch_size, addr);
    return true;
}
