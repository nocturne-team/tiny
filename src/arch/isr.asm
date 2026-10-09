; Entry stubs for CPU exceptions 0-31. Each pushes a uniform frame
; (see struct int_frame in idt.h) and calls exception_dispatch(frame), which
; either panics or (for a recognised fault) adjusts the frame and returns.
bits 64
extern exception_dispatch
global isr_stub_table
global rdmsr_safe, rdmsr_safe_insn, rdmsr_safe_fail
global wrmsr_safe, wrmsr_safe_insn, wrmsr_safe_fail

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
    mov rbx, rsp        ; callee-saved: the frame, to undo the alignment below
    and rsp, ~0xF
    call exception_dispatch
    mov rsp, rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16         ; vector and error code
    iretq

; bool rdmsr_safe(uint32_t msr, uint64_t *out): false if RDMSR raised #GP.
; exception_dispatch resumes a #GP at rdmsr_safe_insn at rdmsr_safe_fail.
rdmsr_safe:
    mov ecx, edi
rdmsr_safe_insn:
    rdmsr
    shl rdx, 32
    mov eax, eax
    or rax, rdx
    mov [rsi], rax
    mov eax, 1
    ret
rdmsr_safe_fail:
    xor eax, eax
    ret

; bool wrmsr_safe(uint32_t msr, uint64_t value): false if WRMSR raised #GP.
wrmsr_safe:
    mov ecx, edi
    mov eax, esi
    mov rdx, rsi
    shr rdx, 32
wrmsr_safe_insn:
    wrmsr
    mov eax, 1
    ret
wrmsr_safe_fail:
    xor eax, eax
    ret

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
