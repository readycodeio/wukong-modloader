#include "path.h"

#include <filesystem>


static std::optional<std::filesystem::path> g_base_dir;
static std::optional<std::filesystem::path> g_mod_dir_override;


std::filesystem::path get_base_dir()
{
    if (!g_base_dir.has_value())
    {
        auto path = std::filesystem::current_path();
        // Check if the path ends with "Win64", if not, then append "Binaries\Win64"
        if (path.filename() != "Win64")
        {
            path = path / L"b1" / L"Binaries" / L"Win64";
        }
        
        g_base_dir = path;
    }

    return g_base_dir.value();
}


std::filesystem::path get_mod_dir()
{
    if (g_mod_dir_override.has_value())
        return g_mod_dir_override.value();

    return get_base_dir() / std::filesystem::path(L"CSharpLoader") / L"Mods";
}


void set_mod_dir_override(std::filesystem::path mod_dir)
{
    g_mod_dir_override = std::move(mod_dir);
}
