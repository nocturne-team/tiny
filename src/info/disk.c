#include "info/disk.h"

void disk_init(struct disk_info *d, enum disk_bus bus) {
    *d = (struct disk_info){ 0 };
    d->bus = bus;
    d->power_on_hours = d->power_cycles = DISK_NONE;
    d->bytes_read = d->bytes_written = DISK_NONE;
    d->unsafe_shutdowns = d->media_errors = d->error_entries = DISK_NONE;
    d->realloc = d->pending = d->uncorrectable = DISK_NONE;
}

// Copies printable ASCII, '?' for anything else, then trims spaces at both ends.
static void copy_trim(char *dst, size_t cap, const uint8_t *src, size_t n) {
    if (!cap)
        return;
    size_t start = 0;
    while (start < n && src[start] == ' ')
        start++;
    size_t end = n;
    while (end > start && src[end - 1] == ' ')
        end--;
    size_t len = 0;
    for (size_t i = start; i < end && len < cap - 1; i++)
        dst[len++] = src[i] >= 0x20 && src[i] <= 0x7e ? (char)src[i] : '?';
    while (len && dst[len - 1] == ' ')  // cut in the middle of a padded string
        len--;
    dst[len] = '\0';
}

void ata_string(char *dst, size_t cap, const uint16_t *words, size_t nwords) {
    uint8_t tmp[80];
    if (nwords > sizeof tmp / 2)
        nwords = sizeof tmp / 2;
    for (size_t i = 0; i < nwords; i++) {
        tmp[2 * i] = (uint8_t)(words[i] >> 8);
        tmp[2 * i + 1] = (uint8_t)words[i];
    }
    copy_trim(dst, cap, tmp, nwords * 2);
}

void ata_parse_identify(const uint16_t w[256], struct disk_info *d) {
    ata_string(d->serial, sizeof d->serial, &w[10], 10);
    ata_string(d->firmware, sizeof d->firmware, &w[23], 4);
    ata_string(d->model, sizeof d->model, &w[27], 20);

    d->sector = 512;
    if ((w[106] & 0xC000) == 0x4000 && (w[106] & (1 << 12))) {
        uint32_t words = w[117] | (uint32_t)w[118] << 16;
        if (words >= 256)
            d->sector = words * 2;
    }
    uint64_t sectors = w[60] | (uint64_t)w[61] << 16;
    if ((w[83] & 0xC000) == 0x4000 && (w[83] & (1 << 10))) {  // word valid, LBA48
        uint64_t lba48 = w[100] | (uint64_t)w[101] << 16 | (uint64_t)w[102] << 32 |
                         (uint64_t)w[103] << 48;
        if (lba48)
            sectors = lba48;
    }
    d->bytes = sectors * d->sector;

    uint16_t rate = w[217];
    d->rpm = rate == 1 || (rate >= 0x0401 && rate <= 0xFFFE) ? rate : 0;
    d->smart_supported = w[82] & 1;
    d->smart_enabled = w[85] & 1;
}

static uint64_t le(const uint8_t *p, int n) {
    uint64_t v = 0;
    for (int i = n - 1; i >= 0; i--)
        v = v << 8 | p[i];
    return v;
}

bool ata_parse_smart(const uint8_t data[512], struct disk_info *d) {
    uint8_t sum = 0;
    for (int i = 0; i < 512; i++)
        sum += data[i];
    if (sum)
        return false;
    for (int i = 0; i < 30; i++) {
        const uint8_t *a = &data[2 + i * 12];
        const uint8_t *raw = &a[5];
        switch (a[0]) {
        case 5: d->realloc = le(raw, 4); break;
        case 9: d->power_on_hours = le(raw, 4); break;
        case 12: d->power_cycles = le(raw, 4); break;
        case 194:
            d->temp_c = raw[0];
            d->has_temp = raw[0] > 0 && raw[0] < 128;
            break;
        case 197: d->pending = le(raw, 4); break;
        case 198: d->uncorrectable = le(raw, 4); break;
        default: break;
        }
    }
    d->smart = true;
    return true;
}

uint32_t nvme_parse_ctrl(const uint8_t id[4096], struct disk_info *d) {
    copy_trim(d->serial, sizeof d->serial, &id[4], 20);
    copy_trim(d->model, sizeof d->model, &id[24], 40);
    copy_trim(d->firmware, sizeof d->firmware, &id[64], 8);
    return (uint32_t)le(&id[516], 4);
}

uint64_t nvme_ns_bytes(const uint8_t ns[4096], uint32_t *sector) {
    uint64_t nsze = le(&ns[0], 8);
    uint8_t lbads = ns[128 + (ns[26] & 0x0F) * 4 + 2];
    if (!nsze || lbads < 9 || lbads > 16)
        return 0;
    *sector = 1u << lbads;
    return nsze << lbads;
}

// 128-bit little-endian count of 512 000-byte units, saturated just below DISK_NONE.
static uint64_t data_units(const uint8_t *p) {
    uint64_t lo = le(p, 8), hi = le(p + 8, 8);
    if (hi || lo > (DISK_NONE - 1) / 512000)
        return DISK_NONE - 1;
    return lo * 512000;
}

// 128-bit counter cut to 64 bits, saturated just below DISK_NONE.
static uint64_t count128(const uint8_t *p) {
    uint64_t lo = le(p, 8), hi = le(p + 8, 8);
    return hi || lo == DISK_NONE ? DISK_NONE - 1 : lo;
}

void nvme_parse_smart(const uint8_t log[512], struct disk_info *d) {
    d->crit_warning = log[0];
    uint16_t kelvin = (uint16_t)le(&log[1], 2);
    d->has_temp = kelvin >= 200 && kelvin <= 400;  // 0 = not reported; others are garbage
    d->temp_c = (int)kelvin - 273;
    d->spare = log[3];
    d->spare_thresh = log[4];
    d->used_pct = log[5];
    d->bytes_read = data_units(&log[32]);
    d->bytes_written = data_units(&log[48]);
    d->power_cycles = count128(&log[112]);
    d->power_on_hours = count128(&log[128]);
    d->unsafe_shutdowns = count128(&log[144]);
    d->media_errors = count128(&log[160]);
    d->error_entries = count128(&log[176]);
    d->smart = true;
}

static bool nonzero(uint64_t v) {
    return v != DISK_NONE && v != 0;
}

enum disk_health disk_health(const struct disk_info *d) {
    if (d->bus == DISK_NVME) {
        if (!d->smart)
            return HEALTH_UNKNOWN;
        if ((d->crit_warning & 0x0D) || d->spare < d->spare_thresh)
            return HEALTH_FAILING;
        if (d->used_pct >= 100)
            return HEALTH_WORN;
        if (d->crit_warning)
            return HEALTH_WARNING;
        return HEALTH_OK;
    }
    if (d->bus != DISK_SATA || !d->smart_supported || !d->smart_enabled)
        return HEALTH_UNKNOWN;
    if (d->smart_failed)
        return HEALTH_FAILING;
    if (!d->smart)
        return HEALTH_UNKNOWN;
    if (nonzero(d->realloc) || nonzero(d->pending) || nonzero(d->uncorrectable))
        return HEALTH_WARNING;
    return HEALTH_OK;
}

const char *disk_health_name(enum disk_health h) {
    switch (h) {
    case HEALTH_OK: return "OK";
    case HEALTH_WARNING: return "warning";
    case HEALTH_WORN: return "worn out";
    case HEALTH_FAILING: return "FAILING";
    default: return "unknown";
    }
}
