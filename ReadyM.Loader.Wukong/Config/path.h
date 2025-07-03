#pragma once
#include <filesystem>


std::filesystem::path get_base_dir();
std::filesystem::path get_mod_dir();
void set_mod_dir_override(std::filesystem::path mod_dir);
