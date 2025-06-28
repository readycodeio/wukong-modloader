#pragma once

struct MonoObject
{
    void *vtable;
    void *synchronisation;
};

union MonoError;
struct MonoString;


void* get_mono_runtime_invoke_ptr();
MonoObject* mono_runtime_invoke(void* method, void* obj, void **params, MonoObject **exc);
void* get_mono_object_try_to_string_ptr();
MonoString* mono_object_try_to_string(MonoObject* obj, MonoObject** exc, MonoError* error);
