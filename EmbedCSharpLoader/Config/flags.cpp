#include "flags.h"

#include <filesystem>
#include <windows.h>

#include "Config/path.h"


static std::filesystem::path get_ini_file_path()
{
    return get_mod_base_path() / std::filesystem::path(L"CSharpLoader") / "b1cs.ini";
}


int load_enable_console()
{
    return GetPrivateProfileIntW(L"Settings", L"Console", 0, get_ini_file_path().c_str());
}

int load_enable_jit()
{
    return GetPrivateProfileIntW(L"Settings", L"EnableJit", 1, get_ini_file_path().c_str());
}

int load_enable_develop()
{
    return GetPrivateProfileIntW(L"Settings", L"Develop", 0, get_ini_file_path().c_str());
}
