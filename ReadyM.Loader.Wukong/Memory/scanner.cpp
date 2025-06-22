#include "scanner.h"

#include <cassert>
#include <optional>
#include <vector>
#include <chrono>
#include <windows.h>
#include <stringzilla/stringzilla.h>

#include "Logger/logger.h"


struct ScanItem
{
    std::vector<uint8_t> subpattern;
    size_t offset;
};


static std::vector<ScanItem> compile_pattern(const std::string& pattern)
{
    std::vector<ScanItem> result{};
    ScanItem last_item{};
    size_t offset = 0;

    for (auto it = pattern.begin(); it != pattern.end(); ++it)
    {
        if (*it == '?')
        {
            if (!last_item.subpattern.empty())
            {
                result.push_back(last_item);
                last_item = ScanItem{};
            }

            if (it + 1 != pattern.end() && *(it + 1) == '?')
            {
                ++it;
            }
            
            offset++;
            last_item.offset = offset;
            continue;
        }

        if (*it == ' ')
            continue;

        size_t count = 0;
        auto val = std::stoul(&*it, &count, 16);

        assert(count == 2);
        
        it += static_cast<int32_t>(count - 1);
        offset += count / 2;
        
        last_item.subpattern.push_back(static_cast<uint8_t>(val));
    }

    if (!last_item.subpattern.empty())
    {
        result.push_back(last_item);
    }
    
    return result;
}


uint64_t signature_impl(const char* module_data, size_t module_size, const std::function<bool(const uint8_t*)>& fn, const std::vector<ScanItem>& pattern_items)
{
    log_debug("Signature scan begin");
            
    size_t pattern_size = 0;
    
    for (const auto& item : pattern_items)
    {
        pattern_size = max(pattern_size, item.offset + item.subpattern.size());
    }
    
    assert(!pattern_items.empty());
    assert(!pattern_items[0].subpattern.empty());

    size_t offset = 0;
    uint64_t result = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    while (offset < module_size - pattern_size)
    {
        auto first_subpattern_data = reinterpret_cast<const char*>(pattern_items[0].subpattern.data());
        auto first_subpattern_size = pattern_items[0].subpattern.size();
        auto first_subpattern_offset = pattern_items[0].offset;
        auto candidate = sz_find(module_data + offset + first_subpattern_offset, module_size - offset - first_subpattern_offset, first_subpattern_data, first_subpattern_size);

        if (candidate == nullptr)
            break;

        auto matches_all = true;
        
        for (size_t i = 1; i < pattern_items.size(); ++i)
        {
            const auto& item = pattern_items[i];
            auto subpattern_data = reinterpret_cast<const char*>(item.subpattern.data());
            auto subpattern_size = item.subpattern.size();
            auto subpattern_offset = item.offset;

            if (!sz_equal(candidate + subpattern_offset, subpattern_data, subpattern_size))
            {
                matches_all = false;
                break;
            }
        }

        if (!matches_all)
        {
            offset = candidate - module_data - first_subpattern_offset + 1;
            continue;
        }

        if (!fn(reinterpret_cast<const uint8_t*>(candidate)))
        {
            offset = candidate - module_data - first_subpattern_offset + 1;
            continue;
        }

        result = reinterpret_cast<uint64_t>(candidate);
        break;
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    log_debug("Signature scan end.");
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    if (result != 0)
    {
        log_debug("Signature found at {:x} in {:d} ms (Scan speed: {:f} GB/s)", result, duration_ms,
                  static_cast<double>(result - reinterpret_cast<uint64_t>(module_data)) / (1024 * 1024 * 1024) / (static_cast<double>(duration_ms) / 1000.0));
    }
    else
    {
        log_debug("Signature not found in {:d} ms (Scan speed: {:f} GB/s)", duration_ms, 
                  static_cast<double>(module_size) / (1024 * 1024 * 1024) / (static_cast<double>(duration_ms) / 1000.0));
    }
    
    return result;
}


uint64_t signature_impl(const char* module_name, const std::string& sig, const std::function<bool(const uint8_t*)>& fn)
{
    HMODULE module = GetModuleHandleA(module_name);

    if (!module)
        return 0;

    const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
    const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(module) + dos_header->e_lfanew);

    const auto module_data = reinterpret_cast<const char*>(module);
    const auto module_size = nt_headers->OptionalHeader.SizeOfImage;

    const auto pattern_items = compile_pattern(sig);

    return signature_impl(module_data, module_size, fn, pattern_items);
}


uint64_t signature(const char* module_name, const std::string& sig)
{
    return signature_impl(module_name, sig, [](const uint8_t*) { return true; });
}

uint64_t signature(const std::string& sig)
{
    return signature_impl(nullptr, sig, [](const uint8_t*) { return true; });
}

uint64_t signature(const char* module_name, const std::string& sig, const std::function<bool(const uint8_t*)>& fn)
{
    return signature_impl(module_name, sig, fn);
}

uint64_t signature(const std::string& sig, const std::function<bool(const uint8_t*)>& fn)
{
    return signature_impl(nullptr, sig, fn);
}
