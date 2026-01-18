#include "object.h"

#include <optional>

#include "Logger/logger.h"
#include "Memory/scanner.h"


typedef MonoObject* (*mono_runtime_invoke_t)(void* method, void* obj, void **params, MonoObject **exc);
typedef MonoString* (*mono_object_try_to_string_t)(MonoObject* obj, MonoObject** exc, MonoError* error);


void* get_mono_runtime_invoke_ptr()
{
    static std::optional<void*> s_mono_runtime_invoke_ptr;

    if (!s_mono_runtime_invoke_ptr.has_value())
    {
        auto mono_runtime_invoke = signature(
            "48 89 5c 24 08 "
            "48 89 6c 24 10 "
            "48 89 74 24 18 "
            "48 89 7c 24 20 "
            "41 56 "
            "48 81 ec b0 00 00 00 "
            "48 8d 44 24 30 "
            "48 8b f9 "
            "48 89 44 24 30 "
            "48 8d 4c 24 30 "
            "48 8d 05 ? ? ? ? "
            "49 8b d9 "
            "48 89 44 24 38"
        );

        if (!mono_runtime_invoke)
        {
            log_error_missing_ptr("mono_runtime_invoke");
            s_mono_runtime_invoke_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_runtime_invoke", mono_runtime_invoke);
        s_mono_runtime_invoke_ptr = reinterpret_cast<void*>(mono_runtime_invoke);
    }

    return s_mono_runtime_invoke_ptr.value();
}


MonoObject* mono_runtime_invoke(void* method, void* obj, void **params, MonoObject **exc)
{
    auto func = reinterpret_cast<mono_runtime_invoke_t>(get_mono_runtime_invoke_ptr());
    if (!func)
    {
        log_error("Cannot invoke method because mono_runtime_invoke is not set.");
        return nullptr;
    }

    return func(method, obj, params, exc);
}


void* get_mono_object_try_to_string_ptr()
{
    static std::optional<void*> s_mono_object_try_to_string_ptr;

    if (!s_mono_object_try_to_string_ptr.has_value())
    {
        auto mono_object_try_to_string = signature(
            "48 89 5c 24 08 "
            "48 89 74 24 18 "
            "57 "
            "48 83 ec 30 "
            "49 8b f8 "
            "48 8b da "
            "48 8b f1 "
            "48 85 d2 "
            "75 ? "
            "4c 8d 05 ? ? ? ? "
            "ba b3 21 00 00 "
            "48 8d 0d ? ? ? ? "
            "e8 ? ? ? ?"
        );

        if (!mono_object_try_to_string)
        {
            log_error_missing_ptr("mono_object_try_to_string");
            s_mono_object_try_to_string_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_object_try_to_string", mono_object_try_to_string);
        s_mono_object_try_to_string_ptr = reinterpret_cast<void*>(mono_object_try_to_string);
    }

    return s_mono_object_try_to_string_ptr.value();
}


MonoString* mono_object_try_to_string(MonoObject* obj, MonoObject** exc, MonoError* error)
{
    auto func = reinterpret_cast<mono_object_try_to_string_t>(get_mono_object_try_to_string_ptr());
    if (!func)
    {
        log_error("Cannot invoke method because mono_object_try_to_string is not set.");
        return nullptr;
    }

    return func(obj, exc, error);
}
