#pragma once
#include <cstdint>


enum MonoImageOpenStatus
{
    MONO_IMAGE_OK,
    MONO_IMAGE_ERROR_ERRNO,
    MONO_IMAGE_MISSING_ASSEMBLYREF,
    MONO_IMAGE_IMAGE_INVALID
};


struct MonoAssemblyName
{
    const char *name;
    const char *culture;
    const char *hash_value;
    const uint8_t* public_key;
    uint8_t public_key_token [17];
    uint32_t hash_alg;
    uint32_t hash_len;
    uint32_t flags;
    uint16_t major, minor, build, revision, arch;
    uint8_t without_version;
    uint8_t without_culture;
    uint8_t without_public_key_token;
};
