#pragma once

#include <windows.h>


HANDLE& get_log_file_handle();

bool create_console();
bool init_console_logging();
