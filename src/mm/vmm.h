#pragma once
#include <stdint.h>

// Maps [phys, phys+len) uncached (PCD|PWT, which is UC under Limine's PAT) into a
// window of its own PML4 slot at 0xffffff0000000000. Returns the virtual address
// of `phys`, or NULL when out of page-table pages or window space.
volatile uint8_t *vmm_map_mmio(uint64_t phys, uint64_t len);
