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

ISR_STUB 0
ISR_STUB 1
; …
ISR_STUB 31