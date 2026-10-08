#include "mm/vmm.h"
#include <stdbool.h>
#include <stddef.h>
#include "arch/x86.h"
#include "mm/pages.h"

#define PTE_P (1ull << 0)
#define PTE_W (1ull << 1)
#define PTE_PWT (1ull << 3)
#define PTE_PCD (1ull << 4)
#define PTE_PS (1ull << 7)
#define PTE_ADDR 0x000FFFFFFFFFF000ull
#define WINDOW_BASE 0xffffff0000000000ull  // PML4 slot 510: unused by Limine and the kernel
#define WINDOW_SIZE (1ull << 39)

static uint64_t next_va = WINDOW_BASE;

// The table `table[idx]` points to, created when missing. NULL on a huge page or no memory.
static uint64_t *next_table(uint64_t *table, unsigned idx) {
    if (!(table[idx] & PTE_P)) {
        uint64_t phys = page_alloc();
        if (!phys)
            return NULL;
        table[idx] = phys | PTE_P | PTE_W;
    } else if (table[idx] & PTE_PS) {
        return NULL;
    }
    return phys_virt(table[idx] & PTE_ADDR);
}

volatile uint8_t *vmm_map_mmio(uint64_t phys, uint64_t len) {
    uint64_t off = phys & 0xFFF;
    phys -= off;
    len = (len + off + 0xFFF) & ~0xFFFull;
    if (!len || len > WINDOW_BASE + WINDOW_SIZE - next_va)
        return NULL;
    uint64_t *pml4 = phys_virt(read_cr3() & PTE_ADDR);
    uint64_t va = next_va;
    for (uint64_t i = 0; i < len; i += 0x1000) {
        uint64_t v = va + i;
        uint64_t *pdpt = next_table(pml4, (v >> 39) & 511);
        uint64_t *pd = pdpt ? next_table(pdpt, (v >> 30) & 511) : NULL;
        uint64_t *pt = pd ? next_table(pd, (v >> 21) & 511) : NULL;
        if (!pt)
            return NULL;
        pt[(v >> 12) & 511] = (phys + i) | PTE_P | PTE_W | PTE_PWT | PTE_PCD;
        invlpg(v);
    }
    next_va += len;
    return (volatile uint8_t *)(uintptr_t)(va + off);
}
