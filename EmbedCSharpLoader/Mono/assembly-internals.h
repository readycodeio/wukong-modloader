#pragma once


typedef bool (*MonoAssemblyCandidatePredicate)(MonoAssembly*, void*);


struct MonoAssemblyLoadRequest
{
    /* Assembly Load context that is requesting an assembly. */
    MonoAssemblyContextKind asmctx;
    void *alc;
    /* Predicate to apply to candidate assemblies. Optional. */
    MonoAssemblyCandidatePredicate predicate;
    /* user_data for predicate. Optional. */
    void* predicate_ud;
};


struct MonoAssemblyOpenRequest
{
    MonoAssemblyLoadRequest request;
    /* Assembly that is requesting the wanted assembly. Optional. */
    MonoAssembly *requesting_assembly;
};
