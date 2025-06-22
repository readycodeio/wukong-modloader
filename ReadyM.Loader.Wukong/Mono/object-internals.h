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


struct MonoException
{
    MonoObject object;
    void *class_name;
    void *message;
    MonoObject *_data;
    MonoObject *inner_ex;
    void *help_link;
    /* Stores the IPs and the generic sharing infos
       (vtable/MRGCTX) of the frames. */
    void  *trace_ips;
    void *stack_trace;
    void *remote_stack_trace;
    int32_t remote_stack_index;
    /* Dynamic methods referenced by the stack trace */
    MonoObject *dynamic_methods;
    int32_t hresult;
    void *source;
    MonoObject *serialization_manager;
    MonoObject *captured_traces;
    void *native_trace_ips;
    int32_t caught_in_unmanaged;
};
