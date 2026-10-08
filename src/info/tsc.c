#include "info/tsc.h"
#include "arch/x86.h"
#include "boot.h"
#include "lib/serial.h"

static uint64_t boot_tsc, hz;
static enum tsc_source source;

// Times ~50 ms of PIT channel 2 (mode 0, gated through port 0x61) with the TSC.
static uint64_t pit_calibrate(void) {
    const uint16_t count = 59659;  // 1193182 Hz / 20
    uint8_t p61 = inb(0x61);
    outb(0x61, (uint8_t)((p61 & ~0x02) | 0x01));  // gate on, speaker off
    outb(0x43, 0xB0);                              // ch2, lo/hi byte, mode 0
    outb(0x42, count & 0xFF);
    outb(0x42, count >> 8);
    uint64_t t0 = rdtsc();
    for (uint32_t spins = 0; !(inb(0x61) & 0x20); spins++) {
        if (spins > 200000) {  // ~0.2-0.4 s: no working PIT on this machine
            outb(0x61, p61);
            return 0;
        }
    }
    uint64_t t1 = rdtsc();
    outb(0x61, p61);
    return (t1 - t0) * 20;
}

void tsc_init(void) {
    boot_tsc = rdtsc();
    if (bootinfo.tsc_hz) {
        hz = bootinfo.tsc_hz;
        source = TSC_SRC_LIMINE;
    } else {
        uint32_t max = cpuid(0, 0).eax;
        if (max >= 0x15) {
            struct cpuid_regs r = cpuid(0x15, 0);
            if (r.eax && r.ebx && r.ecx) {
                hz = (uint64_t)r.ecx * r.ebx / r.eax;
                source = TSC_SRC_CPUID;
            }
        }
        if (!hz && max >= 0x16) {
            uint32_t mhz = cpuid(0x16, 0).eax & 0xFFFF;
            if (mhz) {
                hz = (uint64_t)mhz * 1000000;
                source = TSC_SRC_CPUID_BASE;
            }
        }
        if (!hz) {
            hz = pit_calibrate();
            source = hz ? TSC_SRC_PIT : TSC_SRC_NONE;
        }
    }
    klog("tsc: %lu Hz (source %d)", (unsigned long)hz, (int)source);
}

uint64_t tsc_hz(void) { return hz; }
enum tsc_source tsc_source(void) { return source; }

uint64_t tsc_uptime_seconds(void) {
    return hz ? (rdtsc() - boot_tsc) / hz : 0;
}
