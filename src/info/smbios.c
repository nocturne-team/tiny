#include "info/smbios.h"
#include <stddef.h>
#include "lib/fmt.h"
#include "lib/string.h"

#define MAX_TABLE_LEN 0x100000
#define MAX_STRUCTS 512

struct smb_struct {
    const uint8_t *hdr;
    uint8_t flen;             // formatted-area length
    const char *strings;      // first string
    const char *strings_end;  // first byte of the terminating double NUL
};

static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t rd64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }

static bool checksum(const uint8_t *p, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum = (uint8_t)(sum + p[i]);
    return sum == 0;
}

static size_t clamp_len(uint8_t len, size_t lo) {
    return len < lo ? lo : len;
}

static void get_str(const struct smb_struct *s, uint8_t field, char *dst, size_t cap) {
    dst[0] = '\0';
    if (field >= s->flen)
        return;
    uint8_t want = s->hdr[field];
    if (!want)
        return;
    const char *p = s->strings;
    for (unsigned i = 1; p < s->strings_end; i++) {
        size_t n = 0;
        while (p + n < s->strings_end && p[n])
            n++;
        if (i == want) {
            copy_printable(dst, cap, p, n);
            return;
        }
        p += n + 1;
    }
}

static void handle(const struct smb_struct *s, struct smbios_info *out, unsigned *seen) {
    uint8_t type = s->hdr[0];
    if (type > 2 || (*seen & (1u << type)))
        return;
    *seen |= 1u << type;
    switch (type) {
    case 0:
        get_str(s, 4, out->bios_vendor, sizeof out->bios_vendor);
        get_str(s, 5, out->bios_version, sizeof out->bios_version);
        get_str(s, 8, out->bios_date, sizeof out->bios_date);
        break;
    case 1:
        get_str(s, 4, out->sys_vendor, sizeof out->sys_vendor);
        get_str(s, 5, out->sys_product, sizeof out->sys_product);
        get_str(s, 6, out->sys_version, sizeof out->sys_version);
        break;
    case 2:
        get_str(s, 4, out->board_vendor, sizeof out->board_vendor);
        get_str(s, 5, out->board_product, sizeof out->board_product);
        break;
    }
}

static void walk(const uint8_t *t, size_t len, struct smbios_info *out) {
    unsigned seen = 0;
    size_t off = 0;
    for (int n = 0; n < MAX_STRUCTS && off + 4 <= len; n++) {
        uint8_t type = t[off], flen = t[off + 1];
        if (flen < 4 || off + flen > len)
            return;
        size_t str = off + flen, end = str;
        while (end + 1 < len && !(t[end] == 0 && t[end + 1] == 0))
            end++;
        if (end + 1 >= len)
            return;  // string set never terminates inside the table
        struct smb_struct s = { t + off, flen, (const char *)t + str, (const char *)t + end };
        handle(&s, out, &seen);
        if (type == 127)
            return;
        off = end + 2;
    }
}

void smbios_parse(const void *entry32, const void *entry64, uint64_t hhdm, struct smbios_info *out) {
    memset(out, 0, sizeof *out);
    uint64_t table_phys;
    size_t table_len;

    if (entry64 && memcmp(entry64, "_SM3_", 5) == 0) {
        const uint8_t *e = entry64;
        out->is64 = true;
        out->major = e[7];
        out->minor = e[8];
        out->checksum_ok = checksum(e, clamp_len(e[6], 0x18));
        table_len = rd32(e + 12);
        table_phys = rd64(e + 16);
    } else if (entry32 && memcmp(entry32, "_SM_", 4) == 0) {
        const uint8_t *e = entry32;
        out->major = e[6];
        out->minor = e[7];
        out->checksum_ok = checksum(e, clamp_len(e[5], 0x1F));
        table_len = rd16(e + 22);
        table_phys = rd32(e + 24);
    } else {
        return;
    }

    out->present = true;
    if (!table_phys || !table_len)
        return;
    if (table_len > MAX_TABLE_LEN)
        table_len = MAX_TABLE_LEN;
    walk((const uint8_t *)(uintptr_t)(table_phys + hhdm), table_len, out);
}
