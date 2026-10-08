#include "arch/x86.h"
#include "boot.h"
#include "gfx/gfx.h"
#include "info/rtc.h"
#include "lib/serial.h"

static void demo_screen(void) {
    int cols = gfx_cols(), rows = gfx_rows();
    gfx_fill(0, 0, cols, rows, ' ', ATTR(C_LGRAY, C_BLUE));
    gfx_puts(2, 1, "tiny: framebuffer text test", ATTR(C_YELLOW, C_BLUE), cols - 4);
    for (int i = 0; i < 256; i++)
        gfx_put(2 + (i % 32) * 2, 3 + i / 32, (uint8_t)i, ATTR(C_WHITE, C_BLUE));
    gfx_flush();
}

void kmain(void) {
    serial_init();
    klog("tiny: booting");
    if (!boot_init())
        cpu_halt_forever();
    klog("tiny: %s %s, firmware type %d", bootinfo.bl_name ? bootinfo.bl_name : "?",
         bootinfo.bl_version ? bootinfo.bl_version : "?", bootinfo.firmware);

    if (!bootinfo.fb || !gfx_init(bootinfo.fb)) {
        klog("tiny: no usable 32 bpp RGB framebuffer, halting");
        cpu_halt_forever();
    }
    const struct gfx_layout *l = gfx_get_layout();
    klog("gfx: %lux%lu -> %dx%d cells, scale %d", (unsigned long)bootinfo.fb->width,
         (unsigned long)bootinfo.fb->height, l->cols, l->rows, l->scale);

    struct datetime now;
    if (rtc_read(&now, NULL))
        klog("rtc: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day,
             now.hour, now.min, now.sec);
    else
        klog("rtc: not responding");

    demo_screen();
    klog("PROBE OK");
    klog("UI READY");
#ifdef SMOKE
    qemu_debug_exit(0x10);
#endif
    cpu_halt_forever();
}
