PUBLIC g_csharp_loader_x_load_x_callback
PUBLIC csharp_loader_x_load_x_callback_trampoline

; CSharpLoaderDll trampoline code
.data
g_csharp_loader_x_load_x_callback qword 0

.code
; trampoline function to call the user callback
csharp_loader_x_load_x_callback_trampoline:

; --- save original return value (in RAX) ---
push    rax
sub     rsp, 28h

; --- call user callback ---
mov     rax, g_csharp_loader_x_load_x_callback ; load pointer
call    rax                                    ; invoke it

; --- restore original return value ---
add     rsp, 28h
pop     rax

; --- the original replaced epilogue ---
add     rsp, 5e8h
pop     r15
pop     r14
pop     r13
pop     r12
pop     rdi
pop     rsi
pop     rbx
pop     rbp
ret


END
