#pragma once
#include "info/disk.h"

struct pci_device;
struct pci_list;

#define STORAGE_MAX_CTRLS 4  // per kind (NVMe, AHCI)

// Hardware: reads identify and health data from every NVMe and AHCI controller on
// the list. Needs the DMA page pool (pages_init). Read-only towards the media.
void storage_probe(const struct pci_list *pci, struct storage_list *out);

// Hardware, one controller each. NVMe: true when `d` was filled. AHCI: drives found.
bool nvme_probe(const struct pci_device *pd, struct disk_info *d);
size_t ahci_probe(const struct pci_device *pd, struct disk_info *out, size_t max);
