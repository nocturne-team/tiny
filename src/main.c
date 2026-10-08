#include "arch/idt.h"
#include "arch/x86.h"
#include "boot.h"
#include "gfx/gfx.h"
#include "info/cpu.h"
#include "info/mem.h"
#include "info/rtc.h"
#include "input/input.h"
#include "lib/fmt.h"
#include "lib/serial.h"

static void demo_screen(void) {
    int cols = gfx_cols(), rows = gfx_rows();
    gfx_fill(0, 0, cols, rows, ' ', ATTR(C_LGRAY, C_BLUE));
    gfx_puts(2, 1, "tiny: framebuffer text test", ATTR(C_YELLOW, C_BLUE), cols - 4);
    for (int i = 0; i < 256; i++)
        gfx_put(2 + (i % 32) * 2, 3 + i / 32, (uint8_t)i, ATTR(C_WHITE, C_BLUE));
    gfx_flush();
}

static struct cpu_info cpu;

void kmain(void) {
    serial_init();
    klog("tiny: booting");
    if (!boot_init())
        cpu_halt_forever();
    idt_init();
    tsc_init();
    klog("tiny: %s %s, firmware type %d", bootinfo.bl_name ? bootinfo.bl_name : "?",
         bootinfo.bl_version ? bootinfo.bl_version : "?", bootinfo.firmware);

    if (!bootinfo.fb || !gfx_init(bootinfo.fb)) {
        klog("tiny: no usable 32 bpp RGB framebuffer, halting");
        cpu_halt_forever();
    }
    const struct gfx_layout *l = gfx_get_layout();
    klog("gfx: %lux%lu -> %dx%d cells, scale %d", (unsigned long)bootinfo.fb->width,
         (unsigned long)bootinfo.fb->height, l->cols, l->rows, l->scale);
#ifdef SMOKE_PANIC
    __asm__ volatile("ud2");
#endif

    struct datetime now;
    if (rtc_read(&now, NULL))
        klog("rtc: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day,
             now.hour, now.min, now.sec);
    else
        klog("rtc: not responding");

    bool keyboard = input_init();
    cpu_probe(&cpu);
    klog("cpu: %s | %s | family %u model %u stepping %u | %lu logical | %zu features, %zu caches",
         cpu.vendor, cpu.brand, cpu.sig.family, cpu.sig.model, cpu.sig.stepping,
         (unsigned long)cpu.cpu_count, cpu.feature_count, cpu.cache_count);
    if (bootinfo.memmap) {
        struct mem_summary mem;
        char avail[24];
        mem_summarize(bootinfo.memmap->entries, bootinfo.memmap->entry_count, &mem);
        fmt_size(avail, sizeof avail, mem.available);
        klog("mem: %s available, %zu map entries", avail, mem.entries);
    } else {
        klog("mem: no memory map from bootloader");
    }
    demo_screen();
    klog("PROBE OK");
    klog("UI READY");
#ifdef SMOKE
    qemu_debug_exit(0x10);
#endif
    for (;;) {
        struct key_event k;
        if (keyboard && input_poll(&k))
            klog("key: %d '%c'", k.key, k.ch ? k.ch : ' ');
        cpu_pause();
    }
}
