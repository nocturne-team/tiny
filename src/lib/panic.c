#include "lib/panic.h"
#include <stdarg.h>
#include <stdbool.h>
#include "arch/idt.h"
#include "arch/x86.h"
#include "gfx/gfx.h"
#include "lib/fmt.h"
#include "lib/serial.h"

#define A_PANIC ATTR(C_WHITE, C_RED)

static const char *const exception_names[32] = {
    "#DE divide error", "#DB debug", "NMI", "#BP breakpoint",
    "#OF overflow", "#BR bound range exceeded", "#UD invalid opcode", "#NM device not available",
    "#DF double fault", "coprocessor segment overrun", "#TS invalid TSS", "#NP segment not present",
    "#SS stack-segment fault", "#GP general protection", "#PF page fault", "reserved (15)",
    "#MF x87 floating-point", "#AC alignment check", "#MC machine check", "#XM SIMD floating-point",
    "#VE virtualization", "#CP control protection", "reserved (22)", "reserved (23)",
    "reserved (24)", "reserved (25)", "reserved (26)", "reserved (27)",
    "#HV hypervisor injection", "#VC VMM communication", "#SX security", "reserved (31)",
};

static int row;
static bool in_panic;

__attribute__((format(printf, 1, 2))) static void line(const char *fmt, ...) {
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    klog("%s", buf);
    if (gfx_ready())
        gfx_puts(2, row++, buf, A_PANIC, gfx_cols() - 4);
}

static void begin(void) {
    __asm__ volatile("cli");
    if (in_panic) {
        serial_write("PANIC: recursive panic, halting\n");
        cpu_halt_forever();
    }
    in_panic = true;
    row = 1;
    if (gfx_ready())
        gfx_fill(0, 0, gfx_cols(), gfx_rows(), ' ', A_PANIC);
}

__attribute__((noreturn)) static void finish(void) {
    row++;
    line("System halted. Power off or reset the machine.");
    if (gfx_ready())
        gfx_flush();
#ifdef SMOKE
    qemu_debug_exit(0x11);
#endif
    cpu_halt_forever();
}

void panic(const char *fmt, ...) {
    char msg[128];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    begin();
    line("PANIC: %s", msg);
    finish();
}

void panic_exception(const struct int_frame *f) {
    begin();
    const char *name = f->vector < 32 ? exception_names[f->vector] : "unknown";
    line("PANIC: %s (vector %lu, error 0x%lx)", name, f->vector, f->error);
    row++;
    line("RIP %016lx  CS  %04lx  RFLAGS %016lx", f->rip, f->cs, f->rflags);
    line("RSP %016lx  SS  %04lx  CR2    %016lx", f->rsp, f->ss, read_cr2());
    line("RAX %016lx  RBX %016lx  RCX %016lx", f->rax, f->rbx, f->rcx);
    line("RDX %016lx  RSI %016lx  RDI %016lx", f->rdx, f->rsi, f->rdi);
    line("RBP %016lx  R8  %016lx  R9  %016lx", f->rbp, f->r8, f->r9);
    line("R10 %016lx  R11 %016lx  R12 %016lx", f->r10, f->r11, f->r12);
    line("R13 %016lx  R14 %016lx  R15 %016lx", f->r13, f->r14, f->r15);
    finish();
}
