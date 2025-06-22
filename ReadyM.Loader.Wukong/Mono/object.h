#pragma once

struct MonoObject
{
    void *vtable;
    void *synchronisation;
};


void* get_mono_runtime_invoke_ptr();
MonoObject* mono_runtime_invoke(void* method, void* obj, void **params, MonoObject **exc);
