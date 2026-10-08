#pragma once
#include <stdint.h>

// Stack layout built by isr.asm, lowest address first.
struct int_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags, rsp, ss;  // pushed by the CPU
};

// Installs handlers for CPU exceptions 0-31. Interrupts stay disabled.
void idt_init(void);
// Loads an empty IDT and traps: the CPU triple-faults and resets.
__attribute__((noreturn)) void idt_triple_fault(void);
