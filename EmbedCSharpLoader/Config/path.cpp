#include "path.h"

#include <filesystem>


static std::optional<std::filesystem::path> g_mod_base_path;


std::filesystem::path get_mod_base_path()
{
    if (!g_mod_base_path.has_value())
    {
        auto path = std::filesystem::current_path();
        // Check if the path ends with "Win64", if not, then append "Binaries\Win64"
        if (path.filename() != "Win64")
        {
            path = path / L"b1" / L"Binaries" / L"Win64";
        }
        
        g_mod_base_path = path;
    }

    return g_mod_base_path.value();
}
