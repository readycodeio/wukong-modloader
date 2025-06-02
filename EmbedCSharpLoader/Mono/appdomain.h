#pragma once


void** get_mono_get_root_domain_ptr();
void* mono_get_root_domain();
void* get_ves_icall_System_AppDomain_ExecuteAssembly_ptr();
int ves_icall_System_AppDomain_ExecuteAssembly(void* app_domain, void* assembly);
