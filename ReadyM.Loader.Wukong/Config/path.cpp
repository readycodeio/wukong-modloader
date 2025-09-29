#include "path.h"
#include <filesystem>
#include <shlobj_core.h>
#include "Windows/console.h"


static std::optional<std::filesystem::path> g_base_dir;
static std::optional<std::filesystem::path> g_mod_dir_override;

static std::wstring get_loader_directory()
{
    wchar_t* localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &localAppData)))
    {
        std::wstring path = localAppData;
        CoTaskMemFree(localAppData);
        return path + L"\\ReadyM.Launcher\\game_modes\\Black Myth Wukong Co-op";
    }
    return L"";
}

std::filesystem::path get_base_dir()
{
    if (!g_base_dir.has_value())
    {
        g_base_dir = get_loader_directory();
    }

    return g_base_dir.value();
}


std::filesystem::path get_loader_dir()
{
    return get_base_dir() / std::filesystem::path(L"CSharpLoader");
}


std::filesystem::path get_mod_dir()
{
    if (g_mod_dir_override.has_value())
        return g_mod_dir_override.value();

    return get_base_dir() / std::filesystem::path(L"CSharpLoader") / L"Mods";
}


void set_mod_dir_override(const std::filesystem::path& mod_dir)
{
    g_mod_dir_override = mod_dir;
}
