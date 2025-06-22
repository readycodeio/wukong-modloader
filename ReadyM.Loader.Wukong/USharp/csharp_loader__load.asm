PUBLIC g_csharp_loader__load__callback
PUBLIC csharp_loader__load__callback_trampoline

; CSharpLoaderDll trampoline code
.data
g_csharp_loader__load__callback qword 0

.code
; trampoline function to call the user callback
csharp_loader__load__callback_trampoline:

; --- save original return value (in RAX) ---
push    rax
sub     rsp, 8h

; --- call user callback ---
mov     rax, g_csharp_loader__load__callback ; load pointer
call    rax                                  ; invoke it

; --- restore original return value ---
add     rsp, 8h
pop     rax

; --- original epilogue you replaced ---
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
