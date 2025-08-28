[BITS 64]
global syscall_stub
extern syscall_handler
extern syscall_caller

; Обёртка для syscall
syscall_stub:
    ; Сохраняем все регистры
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax

    ; Сохраняем указатель на текущую задачу
    mov [rel syscall_caller], rsp

    ; Подготавливаем аргументы для syscall_handler
    mov rdi, rax    ; syscall number
    mov rsi, [rsp + 0]  ; original rdi (первый аргумент)
    mov rdx, [rsp + 8]  ; original rsi (второй аргумент)
    mov rcx, [rsp + 16] ; original rdx (третий аргумент)
    mov r8, [rsp + 24]  ; original r10 (четвертый аргумент)
    mov r9, [rsp + 32]  ; original r8 (пятый аргумент)
    
    ; Шестой аргумент (r9) уже в правильном регистре
    ; Седьмой аргумент будет передан через стек
    push qword [rsp + 40] ; original r9 (шестой аргумент)

    ; Вызываем обработчик на C
    call syscall_handler

    ; Очищаем стек от седьмого аргумента
    add rsp, 8

    ; Сохраняем возвращаемое значение
    mov [rsp + 48], rax ; сохраняем в стеке для восстановления позже

    ; Восстанавливаем все регистры, кроме rax
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    ; Восстанавливаем возвращаемое значение в rax
    mov rax, [rsp - 8]

    ; Возврат из системного вызова
    sysretq