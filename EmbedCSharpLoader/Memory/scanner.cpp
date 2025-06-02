#include "scanner.h"

#include <vector>
#include <windows.h>


std::uint64_t find_sig_all(const char* module_name, const std::function<bool(std::uint8_t*)>& fn, const std::string& byte_array)
{
    HMODULE module = GetModuleHandleA(module_name);

    if (!module)
        return 0;

    static const auto pattern_to_byte = [](const char* pattern)
    {
        auto bytes = std::vector<int>{};
        const auto start = const_cast<char*>(pattern);
        const auto end = const_cast<char*>(pattern) + std::strlen(pattern);

        for (auto current = start; current < end; ++current)
        {
            if (*current == '?')
            {
                ++current;

                if (*current == '?')
                    ++current;

                bytes.push_back(-1);
            }
            else
            {
                bytes.push_back(std::strtoul(current, &current, 16));
            }
        }
        return bytes;
    };

    const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
    const auto nt_headers =
        reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(module) + dos_header->e_lfanew);

    const auto size_of_image = nt_headers->OptionalHeader.SizeOfImage;
    const auto pattern_bytes = pattern_to_byte(byte_array.c_str());
    const auto scan_bytes = reinterpret_cast<std::uint8_t*>(module);

    const auto pattern_size = pattern_bytes.size();
    const auto pattern_data = pattern_bytes.data();

    for (auto i = 0ul; i < size_of_image - pattern_size; ++i)
    {
        auto found = true;

        for (auto j = 0ul; j < pattern_size; ++j)
        {
            if (scan_bytes[i + j] == pattern_data[j] || pattern_data[j] == -1)
                continue;
            found = false;
            break;
        }
        if (!found)
            continue;
        auto ptr = &scan_bytes[i];
        if (!fn(ptr))
            continue;
        return reinterpret_cast<std::uint64_t>(ptr);
    }

    return 0;
}


uint64_t signature(const char* module_name, const std::string& sig)
{
    return find_sig_all(module_name, [](std::uint8_t*) { return true; }, sig);
}

uint64_t signature(const std::string& sig)
{
    return find_sig_all(nullptr, [](std::uint8_t*) { return true; }, sig);
}

uint64_t signature(const char* module_name, const std::string& sig, const std::function<bool(std::uint8_t*)>& fn)
{
    return find_sig_all(module_name, fn, sig);
}

uint64_t signature(const std::string& sig, const std::function<bool(std::uint8_t*)>& fn)
{
    return find_sig_all(nullptr, fn, sig);
}
