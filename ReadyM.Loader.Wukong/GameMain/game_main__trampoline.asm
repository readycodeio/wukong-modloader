PUBLIC g_game_main_callback
PUBLIC g_game_main_return
PUBLIC game_main_trampoline

; CSharpLoaderDll trampoline code
.data
g_game_main_callback qword 0
g_game_main_return   qword 0

.code
; trampoline function to call the user callback
game_main_trampoline:

; -- restore the rcx jump register
pop     rcx

; -- save registers the same way they were saved in the entry
lea     rsp, [rsp - 98h]
mov     qword ptr [rsp + 20h], rax
mov     qword ptr [rsp + 28h], rcx
mov     qword ptr [rsp + 30h], rdx
mov     qword ptr [rsp + 38h], rbx
mov     qword ptr [rsp + 40h], rbp
mov     qword ptr [rsp + 48h], rsi
mov     qword ptr [rsp + 50h], rdi
mov     qword ptr [rsp + 58h], r8
mov     qword ptr [rsp + 60h], r9
mov     qword ptr [rsp + 68h], r10
mov     qword ptr [rsp + 70h], r11
mov     qword ptr [rsp + 78h], r12
mov     qword ptr [rsp + 80h], r13
mov     qword ptr [rsp + 88h], r14
mov     qword ptr [rsp + 90h], r15

; --- call user callback ---
call    qword ptr [g_game_main_callback]       ; invoke callback

; --- restore registers before jumping out of the trampoline
mov     qword ptr rax, [rsp + 20h]
mov     qword ptr rcx, [rsp + 28h]
mov     qword ptr rdx, [rsp + 30h]
mov     qword ptr rbx, [rsp + 38h]
mov     qword ptr rbp, [rsp + 40h]
mov     qword ptr rsi, [rsp + 48h]
mov     qword ptr rdi, [rsp + 50h]
mov     qword ptr r8,  [rsp + 58h]
mov     qword ptr r9,  [rsp + 60h]
mov     qword ptr r10, [rsp + 68h]
mov     qword ptr r11, [rsp + 70h]
mov     qword ptr r12, [rsp + 78h]
mov     qword ptr r13, [rsp + 80h]
mov     qword ptr r14, [rsp + 88h]
mov     qword ptr r15, [rsp + 90h]

; --- jump to original return address ---
jmp     qword ptr [g_game_main_return]         ; jump to original return address

END
