#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STORAGE_MAX_DISKS 8
#define DISK_NONE UINT64_MAX  // counter not reported by the drive

enum disk_bus { DISK_NVME, DISK_SATA, DISK_ATAPI };
enum disk_health { HEALTH_UNKNOWN, HEALTH_OK, HEALTH_WARNING, HEALTH_WORN, HEALTH_FAILING };

struct disk_info {
    enum disk_bus bus;
    uint8_t pci_bus, pci_dev, pci_fn;
    uint8_t port;        // AHCI port
    uint32_t namespaces; // NVMe: active namespaces counted into `bytes`
    char model[41], serial[21], firmware[9];
    uint64_t bytes;      // 0 when unknown (or removable media)
    uint32_t sector;     // logical sector size in bytes
    uint16_t rpm;        // ATA: 0 unknown, 1 non-rotating (SSD), else RPM

    bool smart;          // health data was read
    bool smart_supported, smart_enabled;  // ATA
    bool smart_failed;   // ATA SMART RETURN STATUS: threshold exceeded
    int temp_c;          // valid when has_temp
    bool has_temp;
    uint64_t power_on_hours, power_cycles;           // DISK_NONE when absent
    // NVMe health log
    uint8_t crit_warning, spare, spare_thresh, used_pct;
    uint64_t bytes_read, bytes_written, unsafe_shutdowns, media_errors, error_entries;
    // ATA SMART attributes 5, 197, 198 (raw values)
    uint64_t realloc, pending, uncorrectable;
};

struct storage_list {
    bool probed;         // false: nothing was looked at (no DMA memory, or disabled)
    bool disabled;       // "nostorage" on the command line
    size_t count;
    struct disk_info disks[STORAGE_MAX_DISKS];
    size_t nvme_ctrls, ahci_ctrls;
    size_t raid_ctrls, ide_ctrls;  // present but not readable
};

// A disk with every counter set to DISK_NONE and nothing else known.
void disk_init(struct disk_info *d, enum disk_bus bus);

// Pure: ATA strings are big-endian within each word, space padded. Trims both ends.
void ata_string(char *dst, size_t cap, const uint16_t *words, size_t nwords);
// Pure: IDENTIFY DEVICE / IDENTIFY PACKET DEVICE data (256 words).
void ata_parse_identify(const uint16_t w[256], struct disk_info *d);
// Pure: SMART READ DATA (512 bytes). False (and nothing set) on a bad checksum.
bool ata_parse_smart(const uint8_t data[512], struct disk_info *d);

// Pure: NVMe Identify Controller (4096 bytes): strings; returns the namespace count (NN).
uint32_t nvme_parse_ctrl(const uint8_t id[4096], struct disk_info *d);
// Pure: NVMe Identify Namespace: size in bytes (0 if inactive or odd), *sector = LBA size.
uint64_t nvme_ns_bytes(const uint8_t ns[4096], uint32_t *sector);
// Pure: NVMe SMART / Health Information log page (512 bytes).
void nvme_parse_smart(const uint8_t log[512], struct disk_info *d);

// Pure: overall health from what was read.
enum disk_health disk_health(const struct disk_info *d);
const char *disk_health_name(enum disk_health h);
