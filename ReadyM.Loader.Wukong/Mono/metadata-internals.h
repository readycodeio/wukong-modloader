#pragma once


enum MonoAssemblyContextKind
{
    MONO_ASMCTX_DEFAULT = 0,
    MONO_ASMCTX_REFONLY = 1,
    MONO_ASMCTX_LOADFROM = 2,
    MONO_ASMCTX_INDIVIDUAL = 3,
    MONO_ASMCTX_INTERNAL = 4,
    MONO_ASMCTX_LAST = 4
};


struct MonoAssemblyContext
{
    MonoAssemblyContextKind kind;
};
