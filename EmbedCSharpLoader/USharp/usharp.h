#pragma once
#include <cstdint>
#include <string>

int32_t* get_usharp_use_system_env_var_switch_ptr();
bool usharp_use_system_env_var_switch(bool enable);

void* get_mono_sbd_env_options_ptr();
bool set_mono_sbd_env_options(const std::string& debugger_agent_opts);

bool can_enable_debugger();
bool init_debugger(const std::string& log_level, const std::string& log_mask, const std::string& debugger_agent_opts);

bool intercept_csharp_loader__load_runtime(void(*callback)());
