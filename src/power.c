#include "power.h"
#include <stdbool.h>
#include "arch/idt.h"
#include "arch/x86.h"
#include "gfx/gfx.h"
#include "info/acpi.h"
#include "lib/serial.h"

static struct {
    uint16_t smi_cmd;
    uint8_t acpi_enable;
    uint16_t pm1a_cnt, pm1b_cnt;
    bool reset_io;
    uint16_t reset_port;
    uint8_t reset_value;
    bool s5;
    uint8_t slp_typa, slp_typb;
} pw;

static void delay_ms(unsigned ms) {
    for (unsigned i = 0; i < ms * 1000; i++)
        io_wait();
}

void power_init(const struct acpi_info *acpi) {
    const struct acpi_table *fadt = acpi_find(acpi, "FACP");
    if (fadt) {
        const uint8_t *f = fadt->ptr;
        if (fadt->length >= 72) {
            pw.smi_cmd = (uint16_t)acpi_rd32(f + 48);
            pw.acpi_enable = f[52];
            pw.pm1a_cnt = (uint16_t)acpi_rd32(f + 64);
            pw.pm1b_cnt = (uint16_t)acpi_rd32(f + 68);
        }
        // RESET_REG_SUP (flags bit 10) with the reset register in system I/O space.
        if (fadt->length >= 129 && (acpi_rd32(f + 112) & (1u << 10)) && f[116] == 1) {
            pw.reset_io = true;
            pw.reset_port = (uint16_t)acpi_rd64(f + 120);
            pw.reset_value = f[128];
        }
    }
    const struct acpi_table *dsdt = acpi_find(acpi, "DSDT");
    if (dsdt && dsdt->length > 36)
        pw.s5 = acpi_find_s5((const uint8_t *)dsdt->ptr + 36, dsdt->length - 36, &pw.slp_typa, &pw.slp_typb);
    klog("power: PM1a_CNT 0x%x, S5 %s, reset register %s", pw.pm1a_cnt,
         pw.s5 ? "found" : "missing", pw.reset_io ? "I/O" : "none");
}

void power_reboot(void) {
    klog("power: rebooting");
    if (pw.reset_io) {
        klog("power: ACPI reset register 0x%x <- 0x%x", pw.reset_port, pw.reset_value);
        outb(pw.reset_port, pw.reset_value);
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
    if (inw(pw.pm1a_cnt) & 1)
        return;  // SCI_EN already set
    if (!pw.smi_cmd || !pw.acpi_enable)
        return;
    outb(pw.smi_cmd, pw.acpi_enable);
    for (int i = 0; i < 300 && !(inw(pw.pm1a_cnt) & 1); i++)
        delay_ms(1);
}

void power_shutdown(void) {
    klog("power: shutting down");
    if (pw.s5 && pw.pm1a_cnt) {
        enter_acpi_mode();
        klog("power: ACPI S5 (SLP_TYPa=%u SLP_TYPb=%u)", pw.slp_typa, pw.slp_typb);
        outw(pw.pm1a_cnt, (uint16_t)(((pw.slp_typa & 7) << 10) | (1 << 13)));
        if (pw.pm1b_cnt)
            outw(pw.pm1b_cnt, (uint16_t)(((pw.slp_typb & 7) << 10) | (1 << 13)));
        delay_ms(500);
    }
    klog("power: trying emulator shutdown ports");
    outw(0x604, 0x2000);   // QEMU q35 / newer pc
    outw(0xB004, 0x2000);  // Bochs, older QEMU
    outw(0x4004, 0x3400);  // VirtualBox
    delay_ms(500);

    klog("power: shutdown failed, halting");
    if (gfx_ready()) {
        gfx_fill(0, 0, gfx_cols(), gfx_rows(), ' ', ATTR(C_WHITE, C_BLACK));
        gfx_puts(2, 1, "It is now safe to turn off your computer.", ATTR(C_WHITE, C_BLACK), gfx_cols() - 4);
        gfx_flush();
    }
    cpu_halt_forever();
}
