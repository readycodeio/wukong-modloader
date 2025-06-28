#pragma once

#include <windows.h>


extern HANDLE g_log_file_handle;


bool create_console();
bool init_console_logging();
