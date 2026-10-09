#pragma once
#include <stdbool.h>
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }
static inline void outw(uint16_t port, uint16_t v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port)); }
static inline uint16_t inw(uint16_t port) { uint16_t v; __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port)); return v; }

// A write to the POST port takes roughly 1 µs; used for short delays.
static inline void io_wait(void) { outb(0x80, 0); }

struct cpuid_regs { uint32_t eax, ebx, ecx, edx; };

static inline struct cpuid_regs cpuid(uint32_t leaf, uint32_t sub) {
    struct cpuid_regs r;
    __asm__ volatile("cpuid" : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx) : "a"(leaf), "c"(sub));
    return r;
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void cpu_pause(void) { __asm__ volatile("pause"); }

static inline uint64_t read_cr2(void) { uint64_t v; __asm__ volatile("mov %%cr2, %0" : "=r"(v)); return v; }
static inline uint16_t read_cs(void) { uint16_t v; __asm__ volatile("mov %%cs, %0" : "=r"(v)); return v; }

__attribute__((noreturn)) static inline void cpu_halt_forever(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

// Exits QEMU when started with -device isa-debug-exit,iobase=0xf4 (status = code*2+1).
static inline void qemu_debug_exit(uint8_t code) { outb(0xf4, code); }

// RDMSR that returns false instead of faulting on an MSR the CPU doesn't have (isr.asm).
// Needs idt_init().
bool rdmsr_safe(uint32_t msr, uint64_t *out);
// WRMSR that returns false instead of faulting.
bool wrmsr_safe(uint32_t msr, uint64_t value);

static inline uint64_t read_cr3(void) { uint64_t v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v; }
static inline void invlpg(uint64_t va) { __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory"); }
// Orders earlier stores (ring/context writes) before later ones (doorbells).
static inline void wmb(void) { __asm__ volatile("sfence" : : : "memory"); }
