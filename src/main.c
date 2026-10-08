#include "arch/idt.h"
#include "arch/x86.h"
#include "boot.h"
#include "gfx/gfx.h"
#include "info/cpu.h"
#include "info/fw.h"
#include "info/mem.h"
#include "info/pci.h"
#include "info/tsc.h"
#include "input/input.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "mm/pages.h"
#include "power.h"
#include "ui/page.h"
#include "ui/tabs.h"
#include "ui/ui.h"

static struct cpu_info cpu;
static struct fw_info fw;
static struct pci_list pci;
static struct page pages[5];

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
    pci_scan(pci_config_read, &pci);
    klog("pci: %zu devices%s", pci.count, pci.truncated ? " (truncated)" : "");
    pages_init();
    fw_probe(&fw);
    power_init(&fw.acpi);

    size_t width = (size_t)gfx_cols() - 4;
    tab_cpu_build(&pages[0], &cpu, width);
    tab_mem_build(&pages[1], bootinfo.memmap, &fw.smbios);
    tab_pci_build(&pages[2], &pci);
    tab_fw_build(&pages[3], &fw);
    tab_about_build(&pages[4]);
    klog("PROBE OK");

    ui_run(pages, 5, keyboard);
}
