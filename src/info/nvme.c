#include "info/storage.h"
#include "arch/x86.h"
#include "info/pci.h"
#include "info/tsc.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "mm/mmio.h"
#include "mm/pages.h"

// Controller registers
#define REG_CAP 0x00
#define REG_VS 0x08
#define REG_CC 0x14
#define REG_CSTS 0x1C
#define REG_AQA 0x24
#define REG_ASQ 0x28
#define REG_ACQ 0x30
#define CC_EN (1u << 0)
#define CC_SHN_NORMAL (1u << 14)
#define CC_IOSQES (6u << 16)
#define CC_IOCQES (4u << 20)
#define CSTS_RDY (1u << 0)
#define CSTS_CFS (1u << 1)
#define CSTS_SHST_MASK (3u << 2)
#define CSTS_SHST_DONE (2u << 2)

#define QUEUE_LEN 16
#define CMD_TIMEOUT_MS 1000
#define MAX_NS 4

enum { OP_GET_LOG = 0x02, OP_IDENTIFY = 0x06 };

struct sqe {
    uint32_t dw0, nsid, rsvd[2];
    uint64_t mptr, prp1, prp2;
    uint32_t dw10, dw11, dw12, dw13, dw14, dw15;
};

struct cqe {
    uint32_t dw0, dw1, dw2, dw3;  // dw3: CID 15:0, phase 16, status 31:17
};

struct nvme {
    char name[8];
    volatile uint8_t *regs;
    uint64_t len;
    uint32_t dstrd;  // doorbell stride in bytes
    struct sqe *sq;
    volatile struct cqe *cq;
    uint16_t sq_tail, cq_head, cid;
    uint32_t phase;
    uint64_t buf_phys;
    uint8_t *buf;
};

static void doorbell(struct nvme *n, unsigned idx, uint32_t v) {
    uint32_t off = 0x1000 + idx * n->dstrd;
    if (off + 4 <= n->len)
        mmio_wr(n->regs, off, v);
}

// Submits one admin command with the data buffer as PRP1 and polls for its completion.
static bool admin(struct nvme *n, uint8_t op, uint32_t nsid, uint32_t dw10, const char *what) {
    struct sqe *s = &n->sq[n->sq_tail];
    *s = (struct sqe){ 0 };
    uint16_t cid = n->cid++;
    s->dw0 = op | (uint32_t)cid << 16;
    s->nsid = nsid;
    s->prp1 = n->buf_phys;
    s->dw10 = dw10;
    n->sq_tail = (uint16_t)((n->sq_tail + 1) % QUEUE_LEN);
    wmb();
    doorbell(n, 0, n->sq_tail);

    for (unsigned i = 0; i <= CMD_TIMEOUT_MS * 100; i++) {
        volatile struct cqe *c = &n->cq[n->cq_head];
        uint32_t dw3 = c->dw3;
        if (((dw3 >> 16) & 1) == n->phase) {
            if (++n->cq_head == QUEUE_LEN) {
                n->cq_head = 0;
                n->phase ^= 1;
            }
            doorbell(n, 1, n->cq_head);
            uint32_t status = (dw3 >> 17) & 0x7FFF;
            if ((dw3 & 0xFFFF) != cid || status) {
                klog("nvme: %s: %s failed, status 0x%x", n->name, what, status);
                return false;
            }
            return true;
        }
        tsc_delay_us(10);
    }
    klog("nvme: %s: %s timeout", n->name, what);
    return false;
}

static bool disable(struct nvme *n, unsigned timeout_ms) {
    uint32_t cc = mmio_rd(n->regs, REG_CC);
    if (cc & CC_EN) {
        // Clearing EN before RDY is set is undefined: let a pending enable finish first.
        mmio_wait(n->regs, REG_CSTS, CSTS_RDY, CSTS_RDY, timeout_ms);
        mmio_wr(n->regs, REG_CC, cc & ~CC_EN);
    }
    return mmio_wait(n->regs, REG_CSTS, CSTS_RDY, 0, timeout_ms);
}

static bool start(struct nvme *n, const struct pci_device *pd, unsigned timeout_ms) {
    if (!disable(n, timeout_ms)) {
        klog("nvme: %s: reset timeout", n->name);
        return false;
    }
    pci_bus_master(pd);  // only now: a controller left running could DMA into our memory
    uint64_t sq = page_alloc(), cq = page_alloc();
    n->buf_phys = page_alloc();
    if (!sq || !cq || !n->buf_phys) {
        klog("nvme: %s: out of DMA pages", n->name);
        return false;
    }
    n->sq = phys_virt(sq);
    n->cq = phys_virt(cq);
    n->buf = phys_virt(n->buf_phys);
    n->phase = 1;
    mmio_wr(n->regs, REG_AQA, (QUEUE_LEN - 1) << 16 | (QUEUE_LEN - 1));
    mmio_wr64(n->regs, REG_ASQ, sq);
    mmio_wr64(n->regs, REG_ACQ, cq);
    mmio_wr(n->regs, REG_CC, CC_IOCQES | CC_IOSQES | CC_EN);  // NVM command set, 4 KiB pages
    for (unsigned i = 0; i <= timeout_ms * 100; i++) {
        uint32_t st = mmio_rd(n->regs, REG_CSTS);
        if (st & CSTS_CFS) {
            klog("nvme: %s: controller fatal status", n->name);
            mmio_wr(n->regs, REG_CC, 0);
            return false;
        }
        if (st & CSTS_RDY)
            return true;
        tsc_delay_us(10);
    }
    klog("nvme: %s: enable timeout", n->name);
    mmio_wr(n->regs, REG_CC, 0);
    return false;
}

// Normal shutdown, so powering off from tiny doesn't count as an unsafe shutdown.
static void shutdown(struct nvme *n) {
    uint32_t cc = mmio_rd(n->regs, REG_CC);
    mmio_wr(n->regs, REG_CC, (cc & ~(3u << 14)) | CC_SHN_NORMAL);
    if (!mmio_wait(n->regs, REG_CSTS, CSTS_SHST_MASK, CSTS_SHST_DONE, 2000))
        klog("nvme: %s: shutdown notification timeout", n->name);
}

bool nvme_probe(const struct pci_device *pd, struct disk_info *d) {
    struct nvme n = { 0 };
    ksnprintf(n.name, sizeof n.name, "%02x:%02x.%x", pd->bus, pd->dev, pd->fn);
    n.regs = pci_map_bar(pd, 0, 0x4000, &n.len);
    if (!n.regs || n.len < 0x1008) {
        klog("nvme: %s: BAR0 not usable", n.name);
        return false;
    }
    uint64_t cap = mmio_rd(n.regs, REG_CAP) | (uint64_t)mmio_rd(n.regs, REG_CAP + 4) << 32;
    uint32_t vs = mmio_rd(n.regs, REG_VS);
    unsigned timeout_ms = (unsigned)((cap >> 24) & 0xFF) * 500;
    if (timeout_ms < 500)
        timeout_ms = 500;
    n.dstrd = 4u << ((cap >> 32) & 0xF);
    if (!((cap >> 37) & 1) || ((cap >> 48) & 0xF)) {
        klog("nvme: %s: no NVM command set or 4 KiB pages, skipped", n.name);
        return false;
    }
    if (!start(&n, pd, timeout_ms))
        return false;

    disk_init(d, DISK_NVME);
    d->pci_bus = pd->bus;
    d->pci_dev = pd->dev;
    d->pci_fn = pd->fn;
    bool ok = admin(&n, OP_IDENTIFY, 0, 1, "identify controller");
    uint32_t nn = ok ? nvme_parse_ctrl(n.buf, d) : 0;
    for (uint32_t ns = 1; ok && ns <= nn && ns <= MAX_NS; ns++) {
        uint32_t sector = 0;
        if (!admin(&n, OP_IDENTIFY, ns, 0, "identify namespace"))
            break;
        uint64_t bytes = nvme_ns_bytes(n.buf, &sector);
        if (bytes) {
            d->bytes += bytes;
            d->sector = sector;
            d->namespaces++;
        }
    }
    if (ok && admin(&n, OP_GET_LOG, 0xFFFFFFFF, 0x02 | (512 / 4 - 1) << 16, "SMART log"))
        nvme_parse_smart(n.buf, d);
    klog("nvme: %s v%u.%u, %u active namespace%s", n.name, vs >> 16, (vs >> 8) & 0xFF,
         d->namespaces, d->namespaces == 1 ? "" : "s");
    shutdown(&n);
    return ok;
}
