#include "domain.h"

#include <optional>

#include "Logger/logger.h"
#include "Memory/scanner.h"
#include "Mono/assembly.h"
#include "Mono/appdomain.h"


typedef MonoAssembly* (*mono_domain_assembly_open_t)(void* domain, const char* name);

static std::optional<void*> g_mono_domain_assembly_open_ptr;


void* get_mono_domain_assembly_open_ptr()
{
    if (!g_mono_domain_assembly_open_ptr.has_value())
    {
        auto mono_domain_assembly_open = signature(
            "4c 8b dc "
            "49 89 5b 08 "
            "49 89 6b 10 "
            "49 89 73 18 "
            "57 "
            "48 83 ec 60 "
            "49 8d 43 b8 "
            "48 8b f1 "
            "49 89 43 b8 "
            "49 8d 4b b8"
        );
        
        if (!mono_domain_assembly_open)
        {
            log_error_missing_ptr("mono_domain_assembly_open");
            g_mono_domain_assembly_open_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_domain_assembly_open", mono_domain_assembly_open);
        g_mono_domain_assembly_open_ptr = reinterpret_cast<void*>(mono_domain_assembly_open);
    }

    return g_mono_domain_assembly_open_ptr.value();
}


MonoAssembly* mono_domain_assembly_open(void* domain, const char* name)
{
    auto func = reinterpret_cast<mono_domain_assembly_open_t>(get_mono_domain_assembly_open_ptr());
    if (!func)
    {
        log_error("Cannot open assembly because get_mono_domain_assembly_open_ptr is not set.");
        return nullptr;
    }
    
    return func(domain, name);
}
