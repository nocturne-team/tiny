#include "mm/mmio.h"
#include "info/pci.h"
#include "info/tsc.h"
#include "mm/vmm.h"

bool mmio_wait(volatile uint8_t *base, uint32_t off, uint32_t mask, uint32_t want, unsigned ms) {
    for (unsigned i = 0; i <= ms * 100; i++) {
        if ((mmio_rd(base, off) & mask) == want)
            return true;
        tsc_delay_us(10);
    }
    return false;
}

volatile uint8_t *pci_map_bar(const struct pci_device *pd, unsigned bar, uint64_t max, uint64_t *len) {
    uint8_t b = pd->bus, dv = pd->dev, f = pd->fn, reg = (uint8_t)(0x10 + 4 * bar);
    uint32_t cmd = pci_config_read(b, dv, f, 0x04) & 0xFFFF;
    pci_config_write(b, dv, f, 0x04, cmd & ~0x2u);  // memory decode off while sizing
    uint32_t lo = pci_config_read(b, dv, f, reg);
    bool is64 = !(lo & 1) && ((lo >> 1) & 3) == 2 && bar < 5;
    uint32_t hi = is64 ? pci_config_read(b, dv, f, (uint8_t)(reg + 4)) : 0;
    pci_config_write(b, dv, f, reg, 0xFFFFFFFF);
    uint64_t size = pci_bar_size(pci_config_read(b, dv, f, reg));
    pci_config_write(b, dv, f, reg, lo);
    pci_config_write(b, dv, f, 0x04, cmd | 0x6u);  // memory space + bus master
    uint64_t base = pci_bar_base(lo, hi);
    if (!base || size < 0x400)
        return NULL;
    *len = size < max ? size : max;
    return vmm_map_mmio(base, *len);
}
