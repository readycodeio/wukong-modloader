PUBLIC g_csharp_loader__load_runtime_callback
PUBLIC csharp_loader__load_runtime_callback_trampoline

; CSharpLoaderDll trampoline code
.data
g_csharp_loader__load_runtime_callback qword 0

.code
; trampoline function to call the user callback
csharp_loader__load_runtime_callback_trampoline:

; --- save original return value (in RAX) ---
push    rax
sub     rsp, 8h

; --- call user callback ---
mov     rax, g_csharp_loader__load_runtime_callback  ; load pointer
call    rax                                          ; invoke it

; --- restore original return value ---
add     rsp, 8h
pop     rax

; --- original epilogue you replaced ---
add     rsp, 30h
pop     rdi
pop     rsi
pop     rbp
ret

END
