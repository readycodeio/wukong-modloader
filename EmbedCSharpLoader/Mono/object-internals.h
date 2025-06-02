#pragma once
#include <assert.h>

#include "Mono/object.h"


struct MonoReflectionAssembly
{
    MonoObject object;
    void *assembly;
    /* CAS related */
    void *evidence;	/* Evidence */
};


struct MonoMarshalByRefObject
{
    MonoObject obj;
    MonoObject *identity;
};


struct MonoAppDomain
{
    MonoMarshalByRefObject mbr;
    void *data;
};


struct MonoAppDomainHandle
{
    MonoAppDomain * volatile *__raw;

    inline MonoAppDomain * GetRaw () const { return __raw ? *__raw : nullptr; }
    inline MonoAppDomain * volatile * Ref () { assert(__raw); return __raw; }
};


struct MonoReflectionAssemblyHandle
{
    MonoReflectionAssembly * volatile *__raw;

    inline MonoReflectionAssembly * GetRaw () const { return __raw ? *__raw : nullptr; }
    inline MonoReflectionAssembly * volatile * Ref () { assert(__raw); return __raw; }
};
