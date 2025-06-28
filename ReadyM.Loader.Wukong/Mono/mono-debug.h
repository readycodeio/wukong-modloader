#pragma once
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>


enum MonoDebugFormat
{
    MONO_DEBUG_FORMAT_NONE, // == 0
    MONO_DEBUG_FORMAT_MONO, // == 1
    /* Deprecated, the mdb debugger is no longer supported. */
    MONO_DEBUG_FORMAT_DEBUGGER // == 2,
};


struct BundledSymfile
{
    BundledSymfile *next;
    const char *aname;
    const uint8_t *raw_contents;
    int size;
};


void* get_mono_debug_init_ptr();
bool mono_debug_init(MonoDebugFormat format);

BundledSymfile** get_bundled_symfiles_ptr();
bool mono_register_symfile_for_assembly(char* assembly_name, const uint8_t *raw_contents, int size);

bool can_load_symbols();
bool load_debugger_symbols(const std::vector<std::filesystem::path>& dirs);


template<>
struct std::formatter<enum MonoDebugFormat> : std::formatter<std::string_view> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return std::formatter<std::string_view>::parse(ctx);
    }

    auto format(MonoDebugFormat c, std::format_context& ctx) const {
        std::string_view name;
        switch (c) {
        case MONO_DEBUG_FORMAT_NONE: name = "MONO_DEBUG_FORMAT_NONE"; break;
        case MONO_DEBUG_FORMAT_MONO: name = "MONO_DEBUG_FORMAT_MONO"; break;
        case MONO_DEBUG_FORMAT_DEBUGGER: name = "MONO_DEBUG_FORMAT_DEBUGGER"; break;
        default:
            {
                int val = static_cast<std::underlying_type_t<MonoDebugFormat>>(c);
                std::string temp = std::to_string(val);
                return std::formatter<std::string_view>::format(
                    std::string_view{temp}, ctx
                );
            }
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

template<>
struct std::formatter<enum MonoDebugFormat, wchar_t> : std::formatter<std::wstring_view, wchar_t> {
    constexpr auto parse(std::wformat_parse_context& ctx) {
        return std::formatter<std::wstring_view, wchar_t>::parse(ctx);
    }

    auto format(MonoDebugFormat c, std::wformat_context& ctx) const {
        std::wstring_view name;
        switch (c) {
        case MONO_DEBUG_FORMAT_NONE: name = L"MONO_DEBUG_FORMAT_NONE"; break;
        case MONO_DEBUG_FORMAT_MONO: name = L"MONO_DEBUG_FORMAT_MONO"; break;
        case MONO_DEBUG_FORMAT_DEBUGGER: name = L"MONO_DEBUG_FORMAT_DEBUGGER"; break;
        default:
            {
                int val = static_cast<std::underlying_type_t<MonoDebugFormat>>(c);
                std::wstring temp = std::to_wstring(val);
                return std::formatter<std::wstring_view, wchar_t>::format(
                    std::wstring_view{temp}, ctx
                );
            }
        }
        return std::formatter<std::wstring_view, wchar_t>::format(name, ctx);
    }
};
