#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "info/pci.h"

// Takes over every xHCI controller in `pci` and enumerates what is attached.
void usb_init(const struct pci_list *pci);

// Number of USB keyboards currently attached and configured.
size_t usb_keyboards(void);
