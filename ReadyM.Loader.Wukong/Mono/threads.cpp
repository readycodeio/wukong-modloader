#include "threads.h"

#include <optional>

#include "Logger/logger.h"
#include "Memory/scanner.h"


typedef void* (*mono_thread_internal_attach_t)(void* domain);


void* get_mono_thread_internal_attach_ptr()
{
    static std::optional<void*> s_mono_thread_internal_attach_ptr;

    if (!s_mono_thread_internal_attach_ptr.has_value())
    {
        auto mono_thread_internal_attach = signature(
            "40 57 48 83 EC 30 8B 15 ? ? ? ? 48 8B F9 65 48 8B 04 25 58 00 00 00 B9 A8 02 00 00 48 8B 04 D0 48 83 3C 01 00"
        );

        if (!mono_thread_internal_attach)
        {
            log_error_missing_ptr("mono_thread_internal_attach");
            s_mono_thread_internal_attach_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_thread_internal_attach", mono_thread_internal_attach);
        s_mono_thread_internal_attach_ptr = reinterpret_cast<void*>(mono_thread_internal_attach);
    }

    return s_mono_thread_internal_attach_ptr.value();
}


void* mono_thread_internal_attach(void* domain)
{
    auto func = reinterpret_cast<mono_thread_internal_attach_t>(get_mono_thread_internal_attach_ptr());

    if (!func)
    {
        log_error("Cannot attach thread because mono_thread_internal_attach is not set.");
        return nullptr;
    }

    return func(domain);
}
