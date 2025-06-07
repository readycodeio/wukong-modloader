#include "appdomain.h"

#include <optional>

#include "Mono/mono-error.h"
#include "Mono/object-internals.h"
#include "Logger/logger.h"
#include "Memory/scanner.h"



typedef int (*ves_icall_System_AppDomain_ExecuteAssembly_t)(MonoAppDomainHandle ad, MonoReflectionAssemblyHandle refass, void** args, MonoError* error);


static std::optional<void*> g_ves_icall_System_AppDomain_ExecuteAssembly_ptr;
static std::optional<void**> g_domain_ptr;


void** get_mono_get_root_domain_ptr()
{
    if (!g_domain_ptr.has_value())
    {
        auto domain_user_func_ptr = signature(
            "F0 FF 88 B0 00 00 00 48 8B 05 ? ? ? ? 48 3B D8 49 0F 44 C4"
        );
        
        if (!domain_user_func_ptr)
        {
            log_error_missing_ptr("domain_user_func");    
            g_domain_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("domain_user_func", domain_user_func_ptr);

        auto domain_offset = *reinterpret_cast<int*>(domain_user_func_ptr + 10);
        g_domain_ptr = reinterpret_cast<void**>(domain_user_func_ptr + 10 + domain_offset + 4);
        
        log_debug_ptr("domain_ptr", g_domain_ptr.value());
    }

    return g_domain_ptr.value();
}


void* mono_get_root_domain()
{
    auto domain_ptr = get_mono_get_root_domain_ptr();
    if (!domain_ptr)
    {
        log_error("Cannot get root domain pointer because mono_get_root_domain_ptr is not set.");
        return nullptr;
    }
    
    return *domain_ptr;
}


void* get_ves_icall_System_AppDomain_ExecuteAssembly_ptr()
{
    if (!g_ves_icall_System_AppDomain_ExecuteAssembly_ptr.has_value())
    {
        auto ves_icall_System_AppDomain_ExecuteAssembly = signature(
            "48 89 5C 24 08 55 56 57 41 56 41 57 48 83 EC 50 48 89 94 24 88 00 00 00"
        );

        if (!ves_icall_System_AppDomain_ExecuteAssembly)
        {
            log_error_missing_ptr("ves_icall_System_AppDomain_ExecuteAssembly");
            g_ves_icall_System_AppDomain_ExecuteAssembly_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("ves_icall_System_AppDomain_ExecuteAssembly", ves_icall_System_AppDomain_ExecuteAssembly);
        g_ves_icall_System_AppDomain_ExecuteAssembly_ptr = reinterpret_cast<ves_icall_System_AppDomain_ExecuteAssembly_t>(ves_icall_System_AppDomain_ExecuteAssembly);
    }

    return g_ves_icall_System_AppDomain_ExecuteAssembly_ptr.value();
}


int ves_icall_System_AppDomain_ExecuteAssembly(void* app_domain, void* assembly)
{
    auto func = reinterpret_cast<ves_icall_System_AppDomain_ExecuteAssembly_t>(get_ves_icall_System_AppDomain_ExecuteAssembly_ptr());
    if (!func)
    {
        log_error("Cannot execute assembly because ves_icall_System_AppDomain_ExecuteAssembly_ptr is not set.");
        return -1;
    }
    
    MonoAppDomain mono_app_domain{};
    mono_app_domain.data = app_domain;
    MonoAppDomain* mono_app_domain_ptr = &mono_app_domain;
    MonoAppDomainHandle mono_app_domain_handle{&mono_app_domain_ptr};
    MonoReflectionAssembly mono_reflection_assembly{};
    mono_reflection_assembly.assembly = assembly;
    MonoReflectionAssembly* mono_reflection_assembly_ptr = &mono_reflection_assembly;
    MonoReflectionAssemblyHandle mono_reflection_assembly_handle{&mono_reflection_assembly_ptr};
    MonoError mono_error{};
    void* args = nullptr;

    return func(mono_app_domain_handle, mono_reflection_assembly_handle, &args, &mono_error);
}
