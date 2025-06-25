PUBLIC g_bundles_ptr_exported
PUBLIC g_bundled_assemblies_callback
PUBLIC bundled_assemblies_callback_trampoline

; CSharpLoaderDll trampoline code
.data
g_bundles_ptr_exported qword 0
g_bundled_assemblies_callback qword 0

.code
; trampoline function to call the user callback
bundled_assemblies_callback_trampoline:

sub     rsp, 28h
mov     rax, qword ptr [g_bundles_ptr_exported]
mov     qword ptr [rax], rcx
mov     rax, g_bundled_assemblies_callback  ; load pointer
call    rax                                 ; invoke it
add     rsp, 28h
ret

END
