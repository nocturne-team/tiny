#pragma once
#include <stdbool.h>
#include <stdint.h>

// Builds the DMA page pool from the USABLE memory map entries. False when
// there is no HHDM or memory map (USB then stays off).
bool pages_init(void);
// A zeroed 4 KiB page below 4 GiB (physical address), or 0 when out of pages.
uint64_t page_alloc(void);
// HHDM virtual address of a physical address.
void *phys_virt(uint64_t phys);
