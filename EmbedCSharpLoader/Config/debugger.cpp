#include "debugger.h"

#include <string>

#include "Config/config.h"


std::string load_debugger_agent_opts()
{
    return load_param_from_file(L"CSharpLoader/debugger-agent.txt", "transport=dt_socket,loglevel=0,address=127.0.0.1:50446,server=y,suspend=n", "debugger agent opts");   
}

std::string load_log_level()
{
    return load_param_from_file(L"CSharpLoader/mono-log-level.txt", "debug", "Mono debug level");
}

std::string load_log_mask()
{
    return load_param_from_file(L"CSharpLoader/mono-log-mask.txt", "asm,cfg,type", "Mono log mask");
}
