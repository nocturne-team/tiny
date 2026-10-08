#include "ui/about_art.h"
#include "ui/tabs.h"
#include "version.h"

void tab_about_build(struct page *p) {
    page_clear(p);
    p->title = "About";

    for (int row = 0; row < ABOUT_ART_ROWS; row++)
        page_add(p, STYLE_ART, "%c", row + 1);
    page_blank(p);

    page_add(p, STYLE_HEAD, "tiny v%s, a tiny bare-metal x86_64 system info tool", TINY_VERSION);
    page_add(p, STYLE_TEXT, "Boots straight from Limine on BIOS or UEFI with no OS underneath, and");
    page_add(p, STYLE_TEXT, "shows what it finds.");
    page_blank(p);
    page_add(p, STYLE_HEAD, "Credits");
    page_kv(p, "Bootloader", "Limine - github.com/Limine-Bootloader/Limine");
    page_kv(p, "Font", "IBM VGA 8x16, from the SeaBIOS VGA BIOS");
}
