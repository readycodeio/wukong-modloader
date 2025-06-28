#include "logger-config.h"

#include "Config/path.h"


std::filesystem::path get_log_file_path()
{
    return get_base_dir() / "CSharpLog.txt";
}
