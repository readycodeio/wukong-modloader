#pragma once
#include <cstdint>
#include <functional>
#include <windows.h>

#include "Logger/logger.h"

bool patch_set_data(const std::string& module_name, uint64_t rva, const void* patch, int patch_size);
bool patch_set_data(uint64_t addr, const void* patch, int patch_size);
bool patch_data(uint64_t addr, int patch_size, std::function<void()> fn);

template <typename TArg0>
bool patch_call_function(uint64_t addr, TArg0 arg0)
{
    typedef void(*func_t)(TArg0);

    const func_t func = reinterpret_cast<func_t>(addr);
    func(arg0);

    log_debug("Successfully called function at address 0x{:x} with arguments {}", addr, arg0);
    return true;
}


template <typename TArg0>
bool patch_call_function(const std::string& module_name, uint64_t rva, TArg0 arg0)
{
    const auto h_module = GetModuleHandleA(module_name.c_str());
    if (!h_module)
    {
        log_error("Failed to get module handle for {}", module_name);
        return false;
    }

    const auto base_address = reinterpret_cast<BYTE*>(h_module);
    const auto target_address = base_address + rva;

    typedef void(*func_t)(TArg0);

    const func_t func = reinterpret_cast<func_t>(target_address);
    func(arg0);

    log_debug("Successfully called function at address {}+0x{:x} with arguments {}", module_name, rva, arg0);
    return true;
}
