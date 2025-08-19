; isr_stubs.asm — только 0..31
[bits 32]
%macro ISR_STUB 1
  global isr_stub_%1
isr_stub_%1:
  pusha
  ; тут можно обрабатывать или просто iret
  popa
  iretd
%endmacro

%assign i 0
%rep 32
  ISR_STUB i
  %assign i i+1
%endrep

section .note.GNU-stack
; empty