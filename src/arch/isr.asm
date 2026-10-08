; Entry stubs for CPU exceptions 0-31. Each pushes a uniform frame
; (see struct int_frame in idt.h) and calls exception_dispatch(frame).
bits 64
extern exception_dispatch
global isr_stub_table

section .text
isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    cld
    and rsp, ~0xF
    call exception_dispatch
.hang:
    cli
    hlt
    jmp .hang

; Vectors 8, 10-14, 17, 21, 29 and 30 push an error code; the rest get a dummy 0.
%assign i 0
%rep 32
isr_stub_%+i:
%if i == 8 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17 || i == 21 || i == 29 || i == 30
%else
    push 0
%endif
    push i
    jmp isr_common
%assign i i+1
%endrep

section .rodata
isr_stub_table:
%assign i 0
%rep 32
    dq isr_stub_%+i
%assign i i+1
%endrep
