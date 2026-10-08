#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "limine.h"

// Everything the kernel needs from Limine, copied out of the responses once.
// Pointers are NULL and numbers 0 when the bootloader did not answer.
struct bootinfo {
    const struct limine_framebuffer *fb;
    uint64_t hhdm;                                // HHDM offset, 0 if missing
    const struct limine_memmap_response *memmap;
    const void *rsdp;                             // virtual (HHDM) address
    const void *smbios32, *smbios64;              // virtual (HHDM) addresses
    bool have_mp;
    uint64_t cpu_count;
    uint32_t bsp_lapic_id;
    int firmware;                                 // -1 or LIMINE_FIRMWARE_TYPE_*
    const char *bl_name, *bl_version;
    bool have_boot_time;
    int64_t boot_unix;                            // UNIX time at boot
    uint64_t tsc_hz;                              // 0 if missing
    const char *cmdline;                          // executable command line, NULL if missing
};

extern struct bootinfo bootinfo;

// Fills `bootinfo`. Returns false if Limine does not support base revision 6.
bool boot_init(void);
