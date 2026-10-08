#include "mm/pages.h"
#include "boot.h"
#include "lib/serial.h"
#include "lib/string.h"
#include "mm/pool.h"

#define DMA_CAP (4ull << 20)

static struct phys_pool pool;
static bool ready;

bool pages_init(void) {
    if (!bootinfo.hhdm || !bootinfo.memmap) {
        klog("pages: no HHDM or memory map, DMA disabled");
        return false;
    }
    pool_init(&pool, DMA_CAP);
    for (uint64_t i = 0; i < bootinfo.memmap->entry_count; i++) {
        const struct limine_memmap_entry *e = bootinfo.memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
            pool_add(&pool, e->base, e->length);
    }
    klog("pages: %lu KiB usable below 4 GiB", (unsigned long)(pool_avail(&pool) >> 10));
    ready = pool.n > 0;
    return ready;
}

uint64_t page_alloc(void) {
    if (!ready)
        return 0;
    uint64_t phys = pool_alloc(&pool);
    if (phys)
        memset(phys_virt(phys), 0, POOL_PAGE);
    return phys;
}

void *phys_virt(uint64_t phys) {
    return (void *)(uintptr_t)(phys + bootinfo.hhdm);
}
