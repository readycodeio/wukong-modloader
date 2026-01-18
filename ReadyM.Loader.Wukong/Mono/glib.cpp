#include "glib.h"

#include <optional>

#include "Logger/logger.h"
#include "Memory/scanner.h"


void* get_glib_new0_ptr()
{
    static std::optional<void*> s_glib_new0_ptr;

    if (!s_glib_new0_ptr.has_value())
    {
        uint64_t g_new0_ptr_offset = signature(
            "40 53 "
            "48 83 ec 30 "
            "48 8b d9 "
            "48 85 c9 "
            "74 ? "
            "48 8b d1 "
            "b9 01 00 00 00 "
            "ff 15 ? ? ? ? "
            "48 85 c0 "
            "75 ?"
        );
        s_glib_new0_ptr = reinterpret_cast<void*(*)(size_t)>(g_new0_ptr_offset);

        if (s_glib_new0_ptr == nullptr)
        {
            log_error_missing_ptr("glib_new0");
            return nullptr;
        }

        log_debug_ptr("glib_new0", reinterpret_cast<void*>(s_glib_new0_ptr.value()));
    }

    return s_glib_new0_ptr.value();
}


void* glib_new0(size_t size)
{
    auto glib_new0_ptr = reinterpret_cast<void*(*)(size_t)>(get_glib_new0_ptr());;
    if (glib_new0_ptr == nullptr)
    {
        log_error("Cannot allocate memory using glib_new0 ptr is not set.");
        return nullptr;
    }
    
    return glib_new0_ptr(size);
}
