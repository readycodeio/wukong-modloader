#include "debug-helpers.h"

#include "Logger/logger.h"
#include "Memory/scanner.h"


typedef void* (*mono_method_desc_new_t)(const char* name, bool include_namespace);
typedef void* (*mono_method_desc_search_in_image_t)(void* desc, void* image);
typedef void (*mono_method_desc_free_t)(void* desc);


void* get_mono_method_desc_new_ptr()
{
    static std::optional<void*> s_mono_method_desc_new_ptr;

    if (!s_mono_method_desc_new_ptr.has_value())
    {
        auto mono_method_desc_new = signature(
            "40 53 "
            "55 "
            "57 "
            "41 55 "
            "48 83 ec 28 "
            "31 db "
            "41 89 d5 "
            "48 85 c9 "
            "74 ? "
            "48 c7 c0 ff ff ff ff "
            "0f 1f 44 00 00"
        );

        if (!mono_method_desc_new)
        {
            log_error_missing_ptr("mono_method_desc_new");
            s_mono_method_desc_new_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_method_desc_new", mono_method_desc_new);
        s_mono_method_desc_new_ptr = reinterpret_cast<void*>(mono_method_desc_new);
    }

    return s_mono_method_desc_new_ptr.value();
}


void* mono_method_desc_new(const char* name, bool include_namespace)
{
    auto func = reinterpret_cast<mono_method_desc_new_t>(get_mono_method_desc_new_ptr());
    if (!func)
    {
        log_error("Cannot create method descriptor because mono_method_desc_new is not set.");
        return nullptr;
    }

    return func(name, include_namespace);
}


void* get_mono_method_desc_search_in_image_ptr()
{
    static std::optional<void*> s_mono_method_desc_search_in_image_ptr;

    if (!s_mono_method_desc_search_in_image_ptr.has_value())
    {
        auto mono_method_desc_search_in_image = signature(
            "48 89 5c 24 08 "
            "48 89 6c 24 10 "
            "48 89 74 24 18 "
            "57 "
            "41 56 "
            "41 57 "
            "48 81 ec a0 00 00 00 "
            "48 89 d5 "
            "48 89 cb "
            "48 8b 11 "
            "48 85 d2 "
            "75 ? "
            "48 3b 2d ? ? ? ? "
            "75 ? "
            "48 8b 49 08"
        );

        if (!mono_method_desc_search_in_image)
        {
            log_error_missing_ptr("mono_method_desc_search_in_image");
            s_mono_method_desc_search_in_image_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_method_desc_search_in_image", mono_method_desc_search_in_image);
        s_mono_method_desc_search_in_image_ptr = reinterpret_cast<void*>(mono_method_desc_search_in_image);
    }

    return s_mono_method_desc_search_in_image_ptr.value();
}


void* mono_method_desc_search_in_image(void* desc, void* image)
{
    auto func = reinterpret_cast<mono_method_desc_search_in_image_t>(get_mono_method_desc_search_in_image_ptr());
    if (!func)
    {
        log_error("Cannot search method descriptor in image because mono_method_desc_search_in_image is not set.");
        return nullptr;
    }

    return func(desc, image);
}


void* get_mono_method_desc_free_ptr()
{
    static std::optional<void*> s_mono_method_desc_free_ptr;

    if (!s_mono_method_desc_free_ptr.has_value())
    {
        auto mono_method_desc_free = signature(
            "40 53 "
            "48 83 ec 20 "
            "48 89 cb "
            "48 8b 09 "
            "48 85 c9 "
            "75 ? "
            "48 8b 4b 08 "
            "48 85 c9 "
            "74 ?"
        );
        

        if (!mono_method_desc_free)
        {
            log_error_missing_ptr("mono_method_desc_free");
            s_mono_method_desc_free_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mono_method_desc_free", mono_method_desc_free);
        s_mono_method_desc_free_ptr = reinterpret_cast<void*>(mono_method_desc_free);
    }

    return s_mono_method_desc_free_ptr.value();
}


void mono_method_desc_free(void* desc)
{
    auto func = reinterpret_cast<mono_method_desc_free_t>(get_mono_method_desc_free_ptr());
    if (!func)
    {
        log_error("Cannot free method descriptor because mono_method_desc_free is not set.");
        return;
    }

    func(desc);
}
