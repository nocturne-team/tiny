#include "ui/about_art.h"
#include "ui/tabs.h"
#include "version.h"

void tab_about_build(struct page *p) {
    page_clear(p);
    p->title = "About";

    for (int row = 0; row < ABOUT_ART_ROWS; row++)
        page_add(p, STYLE_ART, "%c", row + 1);
    page_blank(p);

    page_add(p, STYLE_HEAD, "tiny v%s - a tiny bare-metal x86_64 system info tool", TINY_VERSION);
    page_blank(p);
    page_add(p, STYLE_TEXT, "tiny boots straight from the Limine bootloader on BIOS or UEFI machines,");
    page_add(p, STYLE_TEXT, "with no operating system underneath. It probes the hardware once - CPUID,");
    page_add(p, STYLE_TEXT, "the memory map, PCI, ACPI, SMBIOS and the CMOS clock - and shows what it");
    page_add(p, STYLE_TEXT, "found in this keyboard-driven text UI.");
    page_blank(p);
    page_add(p, STYLE_TEXT, "It is written in freestanding C: no libc, no interrupts, and no drivers");
    page_add(p, STYLE_TEXT, "beyond a polled PS/2 keyboard and the framebuffer it draws on.");
    page_blank(p);

    page_add(p, STYLE_HEAD, "Keys");
    page_kv(p, "\x1b \x1a", "switch tab (or F1-F5)");
    page_kv(p, "\x18 \x19", "scroll one line");
    page_kv(p, "PgUp PgDn", "scroll one page");
    page_kv(p, "Home End", "jump to the top or bottom");
    page_kv(p, "R, then R", "reboot");
    page_kv(p, "S, then S", "shut down (ACPI S5)");
    page_blank(p);

    page_add(p, STYLE_HEAD, "Credits");
    page_kv(p, "Bootloader", "Limine - github.com/Limine-Bootloader/Limine");
    page_kv(p, "Font", "IBM VGA 8x16, from the SeaBIOS VGA BIOS");
}
