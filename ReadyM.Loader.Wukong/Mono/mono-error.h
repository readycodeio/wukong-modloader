#pragma once
#include <cstdint>


union MonoError
{
    // Merge two uint16 into one uint32 so it can be initialized
    // with one instruction instead of two.
    uint32_t init;
    struct {
        uint16_t error_code;
        uint16_t private_flags; /*DON'T TOUCH */
        void *hidden_1 [12]; /*DON'T TOUCH */
    };
};
