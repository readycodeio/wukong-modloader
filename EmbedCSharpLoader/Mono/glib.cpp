#include "glib.h"

#include <optional>

#include "Logger/logger.h"
#include "Memory/scanner.h"


static std::optional<void*> g_glib_new0_ptr;


void* get_glib_new0_ptr()
{
    if (!g_glib_new0_ptr.has_value())
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
        g_glib_new0_ptr = reinterpret_cast<void*(*)(size_t)>(g_new0_ptr_offset);

        if (g_glib_new0_ptr == nullptr)
        {
            log_error_missing_ptr("g_new0_ptr");
            return nullptr;
        }

        log_debug_ptr("g_new0_ptr", reinterpret_cast<void*>(g_glib_new0_ptr.value()));
    }

    return g_glib_new0_ptr.value();
}


void* glib_new0(size_t size)
{
    auto glib_new0_ptr = reinterpret_cast<void*(*)(size_t)>(get_glib_new0_ptr());;
    if (glib_new0_ptr == nullptr)
    {
        log_error("Cannot allocate memory using g_new0 because g_new0_ptr is not set.");
        return nullptr;
    }
    
    return glib_new0_ptr(size);
}
