#include "arch/idt.h"
#include "arch/x86.h"
#include "lib/panic.h"

struct idt_entry {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t ist;
    uint8_t type;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern const uint64_t isr_stub_table[32];
static struct idt_entry idt[32];

void exception_dispatch(struct int_frame *f);

void idt_init(void) {
    uint16_t cs = read_cs();
    for (int i = 0; i < 32; i++) {
        uint64_t addr = isr_stub_table[i];
        idt[i] = (struct idt_entry){
            .offset_lo = (uint16_t)addr,
            .selector = cs,
            .ist = 0,
            .type = 0x8E,  // present, ring 0, 64-bit interrupt gate
            .offset_mid = (uint16_t)(addr >> 16),
            .offset_hi = (uint32_t)(addr >> 32),
            .zero = 0,
        };
    }
    struct idt_ptr ptr = { sizeof idt - 1, (uint64_t)(uintptr_t)idt };
    __asm__ volatile("lidt %0" : : "m"(ptr));
}

extern const char rdmsr_safe_insn[], rdmsr_safe_fail[], wrmsr_safe_insn[], wrmsr_safe_fail[];

void exception_dispatch(struct int_frame *f) {
    if (f->vector == 13 && f->rip == (uint64_t)(uintptr_t)rdmsr_safe_insn) {
        f->rip = (uint64_t)(uintptr_t)rdmsr_safe_fail;
        return;
    }
    if (f->vector == 13 && f->rip == (uint64_t)(uintptr_t)wrmsr_safe_insn) {
        f->rip = (uint64_t)(uintptr_t)wrmsr_safe_fail;
        return;
    }
    panic_exception(f);
}

void idt_triple_fault(void) {
    struct idt_ptr empty = { 0, 0 };
    __asm__ volatile("lidt %0; int3" : : "m"(empty));
    for (;;)
        __asm__ volatile("hlt");
}
