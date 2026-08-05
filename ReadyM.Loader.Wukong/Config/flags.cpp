#include "flags.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>

#include "Config/path.h"
#include "Logger/logger.h"


static std::filesystem::path get_ini_file_path()
{
    return get_base_dir() / std::filesystem::path(L"CSharpLoader") / "b1cs.ini";
}


// GetPrivateProfile* does not understand a UTF-8 BOM. The three BOM bytes become part of the first line, so
// "[Settings]" never matches and EVERY key silently falls back to its default. It fails without a word: the
// file looks correct, the log looks correct, and the settings are simply ignored. That cost a round trip once
// already, and any player who edits b1cs.ini in an editor that adds a BOM would hit exactly the same thing.
//
// So repair it rather than only complain: strip the BOM in place, and say so. Idempotent, and only ever
// touches the file when those exact three leading bytes are present.
static void repair_ini_encoding()
{
    static bool s_checked = false;
    if (s_checked)
        return;
    s_checked = true;

    const auto path = get_ini_file_path();

    std::string contents;
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return;
        contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    if (contents.size() < 3 || contents.compare(0, 3, "\xEF\xBB\xBF") != 0)
        return;

    contents.erase(0, 3);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        log_error(L"b1cs.ini starts with a UTF-8 BOM, so every setting in it is being ignored, and it could "
                  L"not be rewritten. Re-save {} as ANSI or as UTF-8 without a BOM.", path.wstring());
        return;
    }

    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    out.close();

    log_warn(L"b1cs.ini started with a UTF-8 BOM, which makes Windows ignore every setting in it. Removed the "
             L"BOM from {}; settings will apply from now on.", path.wstring());
}


int load_enable_console()
{
    repair_ini_encoding();
    return GetPrivateProfileIntW(L"Settings", L"Console", 0, get_ini_file_path().c_str());
}

int load_enable_jit()
{
    repair_ini_encoding();
    return GetPrivateProfileIntW(L"Settings", L"EnableJit", 1, get_ini_file_path().c_str());
}

int load_enable_develop()
{
    repair_ini_encoding();
    return GetPrivateProfileIntW(L"Settings", L"Develop", 0, get_ini_file_path().c_str());
}

int load_patch_game_assemblies()
{
    repair_ini_encoding();
    return GetPrivateProfileIntW(L"Settings", L"PatchGameAssemblies", 1, get_ini_file_path().c_str());
}

int load_crash_handler_escalate()
{
    repair_ini_encoding();
    return GetPrivateProfileIntW(L"Settings", L"CrashHandlerEscalate", 1, get_ini_file_path().c_str());
}

std::string load_skip_bundle_replacement()
{
    repair_ini_encoding();

    wchar_t buffer[2048]{};
    const DWORD length = GetPrivateProfileStringW(L"Settings", L"SkipBundleReplacement", L"", buffer,
                                                  static_cast<DWORD>(std::size(buffer)),
                                                  get_ini_file_path().c_str());
    if (length == 0)
        return {};

    // The list is assembly file names, so anything outside ASCII cannot be a valid entry.
    const int size = WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), result.data(), size, nullptr, nullptr);
    return result;
}
