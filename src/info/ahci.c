#include "info/storage.h"
#include "arch/x86.h"
#include "info/pci.h"
#include "info/tsc.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "lib/string.h"
#include "mm/mmio.h"
#include "mm/pages.h"

// HBA registers
#define HBA_CAP 0x00
#define HBA_GHC 0x04
#define HBA_PI 0x0C
#define HBA_VS 0x10
#define HBA_CAP2 0x24
#define HBA_BOHC 0x28
#define GHC_AE (1u << 31)
#define CAP2_BOH (1u << 0)
#define BOHC_BOS (1u << 0)
#define BOHC_OOS (1u << 1)
#define BOHC_BB (1u << 4)
// Port registers
#define PORT(p) (0x100u + 0x80u * (p))
#define P_CLB 0x00
#define P_FB 0x08
#define P_IS 0x10
#define P_IE 0x14
#define P_CMD 0x18
#define P_TFD 0x20
#define P_SIG 0x24
#define P_SSTS 0x28
#define P_SERR 0x30
#define P_CI 0x38
#define CMD_ST (1u << 0)
#define CMD_FRE (1u << 4)
#define CMD_FR (1u << 14)
#define CMD_CR (1u << 15)
#define TFD_ERR (1u << 0)
#define TFD_DRQ (1u << 3)
#define TFD_BSY (1u << 7)
#define IS_TFES (1u << 30)

#define SIG_ATA 0x00000101u
#define SIG_ATAPI 0xEB140101u

enum {
    ATA_IDENTIFY = 0xEC, ATA_IDENTIFY_PACKET = 0xA1, ATA_SMART = 0xB0,
    SMART_READ_DATA = 0xD0, SMART_RETURN_STATUS = 0xDA,
};

#define CMD_TIMEOUT_MS 10000   // SMART on a drive in standby may spin it up first
#define SPINUP_MS 10000        // a drive still spinning up keeps BSY set
#define MAX_PORTS 32

// Per-port DMA page: command list (1 KiB), received FISes (256 B), one command table.
#define OFF_CLB 0x000
#define OFF_FB 0x400
#define OFF_CT 0x800
#define FB_D2H 0x40  // D2H register FIS inside the received-FIS area

struct port {
    volatile uint8_t *r;  // the port's registers
    const char *name;
    unsigned no;
    uint64_t page, data;  // physical
    bool dead;            // a restart after an error failed: send nothing more
};

static bool stop(struct port *p) {
    uint32_t cmd = mmio_rd(p->r, P_CMD);
    if (cmd & CMD_ST)
        mmio_wr(p->r, P_CMD, cmd & ~CMD_ST);
    if (!mmio_wait(p->r, P_CMD, CMD_CR, 0, 500))
        return false;
    cmd = mmio_rd(p->r, P_CMD);
    if (cmd & CMD_FRE)
        mmio_wr(p->r, P_CMD, cmd & ~CMD_FRE);
    return mmio_wait(p->r, P_CMD, CMD_FR, 0, 500);
}

// Points the port at our pages and turns on FIS receive; waits up to `ms` for BSY/DRQ
// to clear (a drive that is still spinning up). The port is stopped before.
static bool receive(struct port *p, unsigned ms) {
    if (!stop(p))
        return false;
    mmio_wr64(p->r, P_CLB, p->page + OFF_CLB);
    mmio_wr64(p->r, P_FB, p->page + OFF_FB);
    mmio_wr(p->r, P_IE, 0);
    mmio_wr(p->r, P_SERR, 0xFFFFFFFF);
    mmio_wr(p->r, P_IS, 0xFFFFFFFF);
    mmio_wr(p->r, P_CMD, mmio_rd(p->r, P_CMD) | CMD_FRE);
    return mmio_wait(p->r, P_TFD, TFD_BSY | TFD_DRQ, 0, ms);
}

static bool start(struct port *p, unsigned ms) {
    if (!receive(p, ms))
        return false;
    mmio_wr(p->r, P_CMD, mmio_rd(p->r, P_CMD) | CMD_ST);
    return true;
}

// Runs one ATA command in slot 0. `len` bytes go to the data page (0: no data).
static bool command(struct port *p, uint8_t cmd, uint8_t feature, uint8_t lba1, uint8_t lba2,
                    uint32_t len, const char *what) {
    if (p->dead)
        return false;
    uint8_t *page = phys_virt(p->page);
    uint32_t *hdr = (uint32_t *)(page + OFF_CLB);
    uint8_t *ct = page + OFF_CT;
    memset(ct, 0, 0x90);
    memset(hdr, 0, 32);
    hdr[0] = 5 | (len ? 1u << 16 : 0);  // 5-dword FIS, PRDT length
    hdr[2] = (uint32_t)(p->page + OFF_CT);
    hdr[3] = (uint32_t)((p->page + OFF_CT) >> 32);

    uint8_t *fis = ct;
    fis[0] = 0x27;  // register H2D
    fis[1] = 0x80;  // command
    fis[2] = cmd;
    fis[3] = feature;
    fis[5] = lba1;
    fis[6] = lba2;
    fis[12] = 1;    // count
    if (len) {
        uint32_t *prd = (uint32_t *)(ct + 0x80);
        prd[0] = (uint32_t)p->data;
        prd[1] = (uint32_t)(p->data >> 32);
        prd[3] = len - 1;
    }

    mmio_wr(p->r, P_IS, 0xFFFFFFFF);
    wmb();
    mmio_wr(p->r, P_CI, 1);
    for (unsigned i = 0; i <= CMD_TIMEOUT_MS * 100; i++) {
        if (mmio_rd(p->r, P_IS) & IS_TFES)
            break;
        if (!(mmio_rd(p->r, P_CI) & 1)) {
            if (mmio_rd(p->r, P_TFD) & TFD_ERR)
                break;
            return true;
        }
        tsc_delay_us(10);
    }
    bool timeout = !(mmio_rd(p->r, P_IS) & IS_TFES) && (mmio_rd(p->r, P_CI) & 1);
    klog("ahci: %s port %u: %s %s (tfd 0x%x)", p->name, p->no, what, timeout ? "timeout" : "error",
         mmio_rd(p->r, P_TFD));
    if (!start(p, CMD_TIMEOUT_MS)) {  // restarting clears the error and any stuck command
        klog("ahci: %s port %u: restart failed, giving up on it", p->name, p->no);
        p->dead = true;
    }
    return false;
}

static void probe_ata(struct port *p, struct disk_info *d) {
    disk_init(d, DISK_SATA);
    uint8_t *data = phys_virt(p->data);
    if (!command(p, ATA_IDENTIFY, 0, 0, 0, 512, "identify"))
        return;
    ata_parse_identify((const uint16_t *)data, d);
    if (!d->smart_supported || !d->smart_enabled)
        return;
    if (command(p, ATA_SMART, SMART_RETURN_STATUS, 0x4F, 0xC2, 0, "SMART status")) {
        const uint8_t *d2h = (const uint8_t *)phys_virt(p->page) + OFF_FB + FB_D2H;
        d->smart_failed = d2h[5] == 0xF4 && d2h[6] == 0x2C;
    }
    if (command(p, ATA_SMART, SMART_READ_DATA, 0x4F, 0xC2, 512, "SMART read data") &&
        !ata_parse_smart(data, d))
        klog("ahci: %s port %u: SMART data checksum mismatch", p->name, p->no);
}

// BIOS/OS handoff (AHCI 1.3.1 section 10.6.3) when the HBA supports it: request
// ownership, give the BIOS 25 ms to say it is busy (BB), and if so up to 2 s more.
static void handoff(volatile uint8_t *hba, const char *name) {
    if (!(mmio_rd(hba, HBA_CAP2) & CAP2_BOH))
        return;
    mmio_wr(hba, HBA_BOHC, mmio_rd(hba, HBA_BOHC) | BOHC_OOS);
    if (mmio_wait(hba, HBA_BOHC, BOHC_BOS, 0, 25))
        return;
    if ((mmio_rd(hba, HBA_BOHC) & BOHC_BB) && mmio_wait(hba, HBA_BOHC, BOHC_BOS, 0, 2000))
        return;
    klog("ahci: %s: bios handoff timeout, continuing", name);
}

size_t ahci_probe(const struct pci_device *pd, struct disk_info *out, size_t max) {
    char name[8];
    ksnprintf(name, sizeof name, "%02x:%02x.%x", pd->bus, pd->dev, pd->fn);
    uint64_t len;
    volatile uint8_t *hba = pci_map_bar(pd, 5, 0x1100, &len);
    if (!hba || len < 0x100) {
        klog("ahci: %s: ABAR not usable", name);
        return 0;
    }
    mmio_wr(hba, HBA_GHC, mmio_rd(hba, HBA_GHC) | GHC_AE);
    handoff(hba, name);
    uint32_t pi = mmio_rd(hba, HBA_PI), vs = mmio_rd(hba, HBA_VS);
    klog("ahci: %s v%u.%u, %u ports (implemented 0x%x)", name, vs >> 16, (vs >> 8) & 0xFF,
         (mmio_rd(hba, HBA_CAP) & 0x1F) + 1, pi);

    // Quiesce every port the firmware may have left running before allowing DMA. A port
    // that won't stop could still write into memory we now own: leave the HBA alone.
    for (unsigned i = 0; i < MAX_PORTS; i++) {
        struct port p = { .r = hba + PORT(i) };
        if ((pi & (1u << i)) && PORT(i) + 0x80 <= len && !stop(&p)) {
            klog("ahci: %s port %u: won't stop, controller skipped", name, i);
            return 0;
        }
    }
    pci_bus_master(pd);

    size_t n = 0;
    for (unsigned i = 0; i < MAX_PORTS && n < max; i++) {
        if (!(pi & (1u << i)) || PORT(i) + 0x80 > len)
            continue;
        struct port p = { .r = hba + PORT(i), .name = name, .no = i };
        uint32_t ssts = mmio_rd(p.r, P_SSTS);
        if ((ssts & 0xF) != 3)  // no device, or no PHY communication
            continue;
        p.page = page_alloc();
        p.data = page_alloc();
        if (!p.page || !p.data) {
            klog("ahci: %s: out of DMA pages", name);
            break;
        }
        // The signature is only valid once the drive's first D2H FIS arrived (BSY clear).
        if (!receive(&p, SPINUP_MS)) {
            klog("ahci: %s port %u: device stays busy, skipped", name, i);
            stop(&p);
            continue;
        }
        uint32_t sig = mmio_rd(p.r, P_SIG);
        if (sig != SIG_ATA && sig != SIG_ATAPI) {
            klog("ahci: %s port %u: unknown device (signature 0x%08x)", name, i, sig);
            stop(&p);
            continue;
        }
        if (!start(&p, CMD_TIMEOUT_MS)) {
            klog("ahci: %s port %u: port start timeout", name, i);
            continue;
        }
        struct disk_info *d = &out[n];
        if (sig == SIG_ATA) {
            probe_ata(&p, d);
        } else {
            disk_init(d, DISK_ATAPI);
            if (command(&p, ATA_IDENTIFY_PACKET, 0, 0, 0, 512, "identify packet"))
                ata_parse_identify(phys_virt(p.data), d);
            d->bytes = 0;  // removable media: IDENTIFY PACKET has no capacity
        }
        if (!d->model[0])
            continue;  // identify failed
        d->pci_bus = pd->bus;
        d->pci_dev = pd->dev;
        d->pci_fn = pd->fn;
        d->port = (uint8_t)i;
        n++;
    }
    return n;
}
