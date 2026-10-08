#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PCI_MAX_DEVICES 128

struct pci_device {
    uint8_t bus, dev, fn;
    uint16_t vendor, device;
    uint8_t class_code, subclass, prog_if, revision;
    uint8_t header_type;  // without the multi-function bit
};

struct pci_list {
    size_t count;
    bool truncated;  // more than PCI_MAX_DEVICES functions
    struct pci_device devices[PCI_MAX_DEVICES];
};

// Reads the 32-bit config register at `off` (dword aligned).
typedef uint32_t (*pci_read_fn)(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off);

// Pure: brute-forces every bus/device, and functions 1-7 of multi-function devices.
void pci_scan(pci_read_fn read, struct pci_list *out);
// Most specific name for a class code, falling back to the class, then "Unknown class".
const char *pci_class_name(uint8_t class_code, uint8_t subclass, uint8_t prog_if);
// Short vendor name, or NULL when not in the built-in table.
const char *pci_vendor_name(uint16_t vendor);

// Hardware: legacy configuration mechanism #1 (ports 0xCF8/0xCFC).
uint32_t pci_config_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off);

// Pure: base address of a memory BAR (`hi` used only when `lo` says 64-bit), 0 for I/O BARs.
uint64_t pci_bar_base(uint32_t lo, uint32_t hi);
// Pure: size of a memory BAR from the value read back after writing all ones, 0 if none.
uint64_t pci_bar_size(uint32_t probe);

// Hardware: writes the 32-bit config register at `off` (dword aligned).
void pci_config_write(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t v);
