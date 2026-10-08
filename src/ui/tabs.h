#pragma once
#include <stddef.h>
#include "limine.h"
#include "ui/page.h"

struct cpu_info;
struct fw_info;
struct pci_list;

// `width` is the usable content width in columns (used to wrap feature flags).
void tab_cpu_build(struct page *p, const struct cpu_info *c, size_t width);
void tab_mem_build(struct page *p, const struct limine_memmap_response *map);
void tab_pci_build(struct page *p, const struct pci_list *l);
void tab_fw_build(struct page *p, const struct fw_info *f);
