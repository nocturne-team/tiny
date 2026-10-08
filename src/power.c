#include "power.h"
#include <stdbool.h>
#include "arch/idt.h"
#include "arch/x86.h"
#include "gfx/gfx.h"
#include "info/acpi.h"
#include "lib/serial.h"

static struct acpi_fadt fadt;
static bool s5;
static uint8_t slp_typa, slp_typb;

static void delay_ms(unsigned ms) {
    for (unsigned i = 0; i < ms * 1000; i++)
        io_wait();
}

void power_init(const struct acpi_info *acpi) {
    acpi_parse_fadt(acpi, &fadt);
    const struct acpi_table *dsdt = acpi_find(acpi, "DSDT");
    if (dsdt && dsdt->length > 36)
        s5 = acpi_find_s5((const uint8_t *)dsdt->ptr + 36, dsdt->length - 36, &slp_typa, &slp_typb);
    klog("power: PM1a_CNT 0x%x, S5 %s, reset register %s", fadt.pm1a_cnt,
         s5 ? "found" : "missing", fadt.reset_io ? "I/O" : "none");
}

void power_reboot(void) {
    klog("power: rebooting");
    if (fadt.reset_io) {
        klog("power: ACPI reset register 0x%x <- 0x%x", fadt.reset_port, fadt.reset_value);
        outb(fadt.reset_port, fadt.reset_value);
        delay_ms(100);
    }
    klog("power: 8042 reset pulse");
    for (int i = 0; i < 100000 && (inb(0x64) & 0x02); i++)
        ;
    outb(0x64, 0xFE);
    delay_ms(100);
    klog("power: reset control register 0xCF9");
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    delay_ms(100);
    klog("power: triple fault");
    idt_triple_fault();
}

static void enter_acpi_mode(void) {
    if (inw(fadt.pm1a_cnt) & 1)
        return;  // SCI_EN already set
    if (!fadt.smi_cmd || !fadt.acpi_enable)
        return;
    outb(fadt.smi_cmd, fadt.acpi_enable);
    for (int i = 0; i < 300 && !(inw(fadt.pm1a_cnt) & 1); i++)
        delay_ms(1);
}

// Like ACPICA: keep the other PM1_CNT bits, write SLP_TYP first, then SLP_EN.
static void sleep_s5(uint16_t port, uint8_t typ) {
    uint16_t v = (uint16_t)(inw(port) & ~0x3C00);
    v |= (uint16_t)((typ & 7) << 10);
    outw(port, v);
    outw(port, (uint16_t)(v | (1 << 13)));
}

void power_shutdown(void) {
    klog("power: shutting down");
    if (s5 && fadt.pm1a_cnt) {
        enter_acpi_mode();
        klog("power: ACPI S5 (SLP_TYPa=%u SLP_TYPb=%u)", slp_typa, slp_typb);
        sleep_s5(fadt.pm1a_cnt, slp_typa);
        if (fadt.pm1b_cnt)
            sleep_s5(fadt.pm1b_cnt, slp_typb);
        delay_ms(500);
    }
    // Emulator-only ports: on real boards these can be live PM registers
    // (0x4004 is PM1_CNT when PMBASE is 0x4000), so only poke them in a VM.
    if (cpuid(1, 0).ecx & (1u << 31)) {
        klog("power: trying emulator shutdown ports");
        outw(0x604, 0x2000);   // QEMU q35 / newer pc
        outw(0xB004, 0x2000);  // Bochs, older QEMU
        outw(0x4004, 0x3400);  // VirtualBox
        delay_ms(500);
    }

    klog("power: shutdown failed, halting");
    if (gfx_ready()) {
        gfx_fill(0, 0, gfx_cols(), gfx_rows(), ' ', ATTR(C_WHITE, C_BLACK));
        gfx_puts(2, 1, "It is now safe to turn off your computer.", ATTR(C_WHITE, C_BLACK), gfx_cols() - 4);
        gfx_flush();
    }
    cpu_halt_forever();
}
