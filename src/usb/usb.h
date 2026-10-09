#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "info/pci.h"
#include "input/input.h"
#include "usb/usb_summary.h"

// Takes over every xHCI controller in `pci` and enumerates what is attached.
void usb_init(const struct pci_list *pci);

// Number of USB keyboards currently attached and configured.
size_t usb_keyboards(void);

// Services every controller (reports, key repeat); returns true and fills *out
// for each key press, one per call.
bool usb_poll(struct key_event *out);

// Next queued mouse event (filled while usb_poll services the controllers).
bool usb_poll_pointer(struct pointer_event *out);

// Changes whenever a device is attached, removed or stops working.
uint32_t usb_generation(void);
// Copies the controllers and the device tree for the USB tab.
void usb_summarize(struct usb_summary *s);
