#include "arch/idt.h"
#include "arch/x86.h"
#include "boot.h"
#include "gfx/gfx.h"
#include "info/cpu.h"
#include "info/fw.h"
#include "info/mem.h"
#include "info/pci.h"
#include "info/sensors.h"
#include "info/storage.h"
#include "info/tsc.h"
#include "input/input.h"
#include "lib/cmdline.h"
#include "lib/fmt.h"
#include "lib/logbuf.h"
#include "lib/serial.h"
#include "mm/pages.h"
#include "power.h"
#include "ui/page.h"
#include "ui/tabs.h"
#include "ui/ui.h"
#include "usb/usb.h"

static struct cpu_info cpu;
static struct fw_info fw;
static struct pci_list pci;
static struct storage_list storage;
static struct sensors sensors;
static struct page pages[10];
static struct usb_summary usb_sum;
static uint32_t usb_gen;
static uint64_t log_seq;

static uint64_t sensors_ms;

// Samples once a second (never when the TSC frequency is unknown).
static bool sensors_update(struct page *p) {
    uint64_t now = tsc_ms();
    if (now - sensors_ms < 1000)
        return false;
    sensors_ms = now;
    sensors_sample(&sensors);
    tab_sensors_build(p, &sensors);
    return true;
}

// Rebuilds when a USB device came, went or stopped working.
static bool usb_update(struct page *p) {
    uint32_t g = usb_generation();
    if (g == usb_gen)
        return false;
    usb_gen = g;
    usb_summarize(&usb_sum);
    tab_usb_build(p, &usb_sum);
    return true;
}

static bool log_update(struct page *p) {
    const struct logbuf *lb = klog_buffer();
    if (lb->seq == log_seq)
        return false;
    log_seq = lb->seq;
    tab_log_build(p, lb);
    return true;
}

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
    bool dma = pages_init();
    if (cmdline_has(bootinfo.cmdline, "nousb")) {
        usb_sum.disabled = true;
        klog("usb: disabled by command line");
    } else if (dma) {
        usb_init(&pci);
    } else {
        usb_sum.no_dma = true;
    }
    if (cmdline_has(bootinfo.cmdline, "nostorage")) {
        storage.disabled = true;
        klog("storage: disabled by command line");
    }
    else if (dma)
        storage_probe(&pci, &storage);
    input_init();
    fw_probe(&fw);
    power_init(&fw.acpi);

    size_t width = (size_t)gfx_cols() - 4;
    tab_cpu_build(&pages[0], &cpu, width);
    tab_mem_build(&pages[1], bootinfo.memmap, &fw.smbios);
    tab_pci_build(&pages[2], &pci);
    if (usb_sum.disabled || usb_sum.no_dma) {
        tab_usb_build(&pages[3], &usb_sum);
    } else {
        usb_gen = usb_generation();
        usb_summarize(&usb_sum);
        tab_usb_build(&pages[3], &usb_sum);
        pages[3].update = usb_update;
    }
    tab_fw_build(&pages[4], &fw);
    tab_storage_build(&pages[5], &storage);
    sensors_init(&sensors, &cpu);
    sensors_sample(&sensors);
    sensors_ms = tsc_ms();
    tab_sensors_build(&pages[6], &sensors);
    pages[6].update = sensors_update;
    pages[7].update = log_update;
    pages[7].follow = true;
    log_update(&pages[7]);
    tab_about_build(&pages[8]);
    tab_keys_init(&pages[9]);
    klog("PROBE OK");

    ui_run(pages, 10);
}
