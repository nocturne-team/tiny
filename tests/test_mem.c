#include "test.h"
#include "info/mem.h"

int main(void) {
    struct limine_memmap_entry e[] = {
        { 0x0, 0x9fc00, LIMINE_MEMMAP_USABLE },
        { 0x9fc00, 0x400, LIMINE_MEMMAP_RESERVED },
        { 0xf0000, 0x10000, LIMINE_MEMMAP_RESERVED },
        { 0x100000, 0x1fe00000, LIMINE_MEMMAP_USABLE },
        { 0x1ff00000, 0x80000, LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE },
        { 0x1ff80000, 0x20000, LIMINE_MEMMAP_ACPI_RECLAIMABLE },
        { 0x1ffa0000, 0x60000, LIMINE_MEMMAP_ACPI_NVS },
        { 0x20000000, 0x100000, LIMINE_MEMMAP_EXECUTABLE_AND_MODULES },
        { 0xfd000000, 0x300000, LIMINE_MEMMAP_FRAMEBUFFER },
        { 0xfee00000, 0x1000, 42 },  // unknown type: counted as an entry only
    };
    struct limine_memmap_entry *ptrs[10];
    for (int i = 0; i < 10; i++)
        ptrs[i] = &e[i];

    struct mem_summary s;
    mem_summarize(ptrs, 10, &s);
    CHECK_EQ(s.entries, 10);
    CHECK_EQ(s.by_type[LIMINE_MEMMAP_USABLE], 0x1fe9fc00);
    CHECK_EQ(s.by_type[LIMINE_MEMMAP_RESERVED], 0x10400);
    CHECK_EQ(s.by_type[LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE], 0x80000);
    CHECK_EQ(s.by_type[LIMINE_MEMMAP_ACPI_NVS], 0x60000);
    CHECK_EQ(s.by_type[LIMINE_MEMMAP_FRAMEBUFFER], 0x300000);
    CHECK_EQ(s.available, 0x2003fc00);
    CHECK_EQ(s.highest, 0x20100000);

    mem_summarize(ptrs, 0, &s);
    CHECK_EQ(s.entries, 0);
    CHECK_EQ(s.available, 0);

    CHECK_STR(mem_type_name(LIMINE_MEMMAP_USABLE), "usable");
    CHECK_STR(mem_type_name(LIMINE_MEMMAP_RESERVED_MAPPED), "reserved (mapped)");
    CHECK_STR(mem_type_name(42), "unknown");
    TEST_MAIN_END();
}
