; syscall_stub.asm  (минимальный безопасный)
[BITS 64]
global syscall_stub
extern syscall_handler
extern g_syscall_kstack_top
extern g_saved_user_rsp

syscall_stub:
    swapgs

    mov [rel g_saved_user_rsp], rsp
    mov rsp, [rel g_syscall_kstack_top]

    push r11
    push rcx
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp

    mov rdi, rax        ; syscall number
    call syscall_handler

    pop rbp
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    pop rcx
    pop r11

    mov rsp, [rel g_saved_user_rsp]
    swapgs
    sysretq
