#pragma once
#include <stddef.h>
#include <stdint.h>
#include "limine.h"

#define MEM_TYPES 9  // LIMINE_MEMMAP_USABLE .. LIMINE_MEMMAP_RESERVED_MAPPED

struct mem_summary {
    uint64_t by_type[MEM_TYPES];  // bytes per Limine memory type
    uint64_t available;           // usable + bootloader + kernel/modules + ACPI reclaimable
    uint64_t highest;             // end of the highest available region
    size_t entries;
};

void mem_summarize(struct limine_memmap_entry *const *entries, uint64_t count, struct mem_summary *out);
const char *mem_type_name(uint64_t type);
