#include "arch/x86.h"
#include "boot.h"
#include "lib/serial.h"

void kmain(void) {
    serial_init();
    serial_write("tiny: booting\n");
    if (!boot_init())
        cpu_halt_forever();

    serial_write("PROBE OK\n");
    serial_write("UI READY\n");
#ifdef SMOKE
    qemu_debug_exit(0x10);
#endif
    cpu_halt_forever();
}
