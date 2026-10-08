#include "arch/x86.h"
#include "info/pci.h"

uint32_t pci_config_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)(dev & 0x1F) << 11) |
                    ((uint32_t)(fn & 0x07) << 8) | (off & 0xFC);
    __asm__ volatile("outl %0, %1" : : "a"(addr), "Nd"((uint16_t)0xCF8));
    uint32_t v;
    __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"((uint16_t)0xCFC));
    return v;
}
