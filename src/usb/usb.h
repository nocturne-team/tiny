#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "info/pci.h"
#include "input/input.h"

// Takes over every xHCI controller in `pci` and enumerates what is attached.
void usb_init(const struct pci_list *pci);

// Number of USB keyboards currently attached and configured.
size_t usb_keyboards(void);

// Services every controller (reports, key repeat); returns true and fills *out
// for each key press, one per call.
bool usb_poll(struct key_event *out);
