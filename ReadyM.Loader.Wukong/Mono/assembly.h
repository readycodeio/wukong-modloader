#pragma once
#include <cstdint>
#include <filesystem>

#include "Mono/image.h"
#include "Mono/glib.h"
#include "Mono/metadata-internals.h"


struct MonoAssembly
{
    int32_t ref_count; /* use atomic operations only */
    char *basedir;
    MonoAssemblyName aname;
    void *image;
    GSList *friend_assembly_names; /* Computed by mono_assembly_load_friends () */
    uint8_t friend_assembly_names_inited;
    uint8_t in_gac;
    uint8_t dynamic;
    uint8_t corlib_internal;
    MonoAssemblyContext context;
    uint8_t wrap_non_exception_throws;
    uint8_t wrap_non_exception_throws_inited;
    uint8_t jit_optimizer_disabled;
    uint8_t jit_optimizer_disabled_inited;
    /* security manager flags (one bit is for lazy initialization) */
    uint32_t ecma:2;		/* Has the ECMA key */
    uint32_t aptc:2;		/* Has the [AllowPartiallyTrustedCallers] attributes */
    uint32_t fulltrust:2;	/* Has FullTrust permission */
    uint32_t unmanaged:2;	/* Has SecurityPermissionFlag.UnmanagedCode permission */
    uint32_t skipverification:2;	/* Has SecurityPermissionFlag.SkipVerification permission */
};


struct MonoBundledAssembly
{
    const char *name;
    const unsigned char *data;
    unsigned int size;
};


void*** get_bundles_ptr();
bool mono_register_bundled_assemblies(MonoBundledAssembly **assemblies);
void* get_mono_register_bundled_assemblies_ptr();
bool intercept_register_bundled_assemblies(void(*callback)());
MonoBundledAssembly** get_mono_register_bundled_assemblies();
bool load_assembly_bundles(std::vector<std::filesystem::path> dirs);

void* get_mono_assembly_request_open_ptr();
void* mono_assembly_request_open(const std::filesystem::path& filename);

void* get_mono_assembly_get_image_ptr();
void* mono_assembly_get_image(MonoAssembly* assembly);
