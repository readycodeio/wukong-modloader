#include "mini-runtime.h"

#include <optional>

#include "Logger/logger.h"
#include "Memory/scanner.h"


void* get_mini_init_ptr()
{
    static std::optional<void*> s_mini_init_ptr;

    if (!s_mini_init_ptr.has_value())
    {
        uint64_t mini_init = signature(
            "40 55 "
            "53 "
            "41 54 "
            "41 55 "
            "41 57 "
            "48 8D 6C 24 A0 "
            "48 81 EC 60 01 00 00 "
            "45 33 ED "
            "4C 8B E2 "
            "44 89 6D F0 "
            "4C 8B F9 "
            "E8 ? ? ? ? "
            "E8 ? ? ? ? "
            "48 8B 0D ? ? ? ? "
            "48 85 C9 "
            "74 ? "
            "FF 15 ? ? ? ?"
        );

        if (mini_init == 0)
        {
            log_error_missing_ptr("mini_init");
            s_mini_init_ptr = nullptr;
            return nullptr;
        }

        log_debug_ptr("mini_init", mini_init);
        s_mini_init_ptr = reinterpret_cast<void*>(mini_init);
    }

    return s_mini_init_ptr.value();
}
