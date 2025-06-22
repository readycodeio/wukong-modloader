PUBLIC g_bundles_ptr_exported
PUBLIC g_register_bundled_assemblies_callback
PUBLIC register_bundled_assemblies_callback_trampoline

; CSharpLoaderDll trampoline code
.data
g_bundles_ptr_exported qword 0
g_register_bundled_assemblies_callback qword 0

.code
; trampoline function to call the user callback
register_bundled_assemblies_callback_trampoline:

sub     rsp, 8h
mov     rax, qword ptr [g_bundles_ptr_exported]
mov     qword ptr [rax], rcx
mov     rax, g_register_bundled_assemblies_callback  ; load pointer
call    rax                                          ; invoke it
add     rsp, 8h
ret

END
