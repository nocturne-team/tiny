#include "arch/x86.h"
#include "boot.h"
#include "lib/serial.h"

void kmain(void) {
    serial_init();
    klog("tiny: booting");
    if (!boot_init())
        cpu_halt_forever();
    klog("tiny: %s %s, firmware type %d", bootinfo.bl_name ? bootinfo.bl_name : "?",
         bootinfo.bl_version ? bootinfo.bl_version : "?", bootinfo.firmware);

    klog("PROBE OK");
    klog("UI READY");
#ifdef SMOKE
    qemu_debug_exit(0x10);
#endif
    cpu_halt_forever();
}
