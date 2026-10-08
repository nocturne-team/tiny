#include "info/acpi.h"
#include "lib/fmt.h"

#define HEADER_LEN 36

bool acpi_checksum(const void *p, size_t len) {
    const uint8_t *b = p;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum = (uint8_t)(sum + b[i]);
    return sum == 0;
}

static bool sane_length(uint32_t len) {
    return len >= HEADER_LEN && len <= ACPI_MAX_LEN;
}

static const uint8_t *virt(uint64_t phys, uint64_t hhdm) {
    return (const uint8_t *)(uintptr_t)(phys + hhdm);
}

static bool add_table(struct acpi_info *out, uint64_t phys, uint64_t hhdm, const struct phys_ranges *m) {
    if (out->table_count == ACPI_MAX_TABLES) {
        out->truncated = true;
        return false;
    }
    struct acpi_table *t = &out->tables[out->table_count++];
    memset(t, 0, sizeof *t);
    if (!phys_ok(m, phys, HEADER_LEN)) {
        memcpy(t->sig, "????", 5);
        return true;
    }
    const uint8_t *hdr = virt(phys, hhdm);
    copy_printable(t->sig, sizeof t->sig, (const char *)hdr, 4);
    copy_printable(t->oem_id, sizeof t->oem_id, (const char *)hdr + 10, 6);
    t->length = acpi_rd32(hdr + 4);
    t->ptr = hdr;
    t->mapped = true;
    t->readable = sane_length(t->length) && phys_ok(m, phys, t->length);
    t->checksum_ok = t->readable && acpi_checksum(hdr, t->length);
    return true;
}

void acpi_parse(const void *rsdp_v, uint64_t hhdm, const struct phys_ranges *m, struct acpi_info *out) {
    memset(out, 0, sizeof *out);
    const uint8_t *rsdp = rsdp_v;
    if (!rsdp || memcmp(rsdp, "RSD PTR ", 8) != 0)
        return;

    out->present = true;
    out->revision = rsdp[15];
    copy_printable(out->oem_id, sizeof out->oem_id, (const char *)rsdp + 9, 6);
    out->rsdp_checksum_ok = acpi_checksum(rsdp, 20) && (out->revision < 2 || acpi_checksum(rsdp, 36));

    uint64_t root_phys = acpi_rd32(rsdp + 16);
    size_t entry_size = 4;
    if (out->revision >= 2 && acpi_rd64(rsdp + 24) != 0) {
        root_phys = acpi_rd64(rsdp + 24);
        entry_size = 8;
        out->use_xsdt = true;
    }
    if (!root_phys)
        return;

    add_table(out, root_phys, hhdm, m);
    const struct acpi_table *root = &out->tables[0];
    if (!root->readable)
        return;

    const uint8_t *base = root->ptr;
    size_t n = (root->length - HEADER_LEN) / entry_size;
    for (size_t i = 0; i < n; i++) {
        const uint8_t *e = base + HEADER_LEN + i * entry_size;
        uint64_t phys = entry_size == 8 ? acpi_rd64(e) : acpi_rd32(e);
        if (phys && !add_table(out, phys, hhdm, m))
            break;
    }

    // The DSDT is referenced from the FADT rather than from the root table.
    const struct acpi_table *fadt = acpi_find(out, "FACP");
    if (fadt) {
        const uint8_t *f = fadt->ptr;
        uint64_t dsdt = fadt->length >= 148 ? acpi_rd64(f + 140) : 0;
        if (!dsdt && fadt->length >= 44)
            dsdt = acpi_rd32(f + 40);
        if (dsdt)
            add_table(out, dsdt, hhdm, m);
    }
}

const struct acpi_table *acpi_find(const struct acpi_info *info, const char *sig) {
    for (size_t i = 0; i < info->table_count; i++)
        if (info->tables[i].readable && memcmp(info->tables[i].sig, sig, 5) == 0)
            return &info->tables[i];
    return NULL;
}

static bool s5_element(const uint8_t *aml, size_t len, size_t *p, uint8_t *val) {
    if (*p >= len)
        return false;
    switch (aml[*p]) {
    case 0x00:  // ZeroOp
    case 0x01:  // OneOp
        *val = aml[*p];
        *p += 1;
        return true;
    case 0x0A:  // BytePrefix
        if (*p + 1 >= len)
            return false;
        *val = aml[*p + 1];
        *p += 2;
        return true;
    case 0x0B:  // WordPrefix
        if (*p + 2 >= len)
            return false;
        *val = aml[*p + 1];
        *p += 3;
        return true;
    case 0x0C:  // DWordPrefix
        if (*p + 4 >= len)
            return false;
        *val = aml[*p + 1];
        *p += 5;
        return true;
    default:
        return false;
    }
}

bool acpi_find_s5(const uint8_t *aml, size_t len, uint8_t *slp_typa, uint8_t *slp_typb) {
    for (size_t i = 1; i + 4 < len; i++) {
        if (memcmp(aml + i, "_S5_", 4) != 0)
            continue;
        // Must be a Name() definition: NameOp, optionally followed by a root prefix.
        bool is_name = aml[i - 1] == 0x08 || (i >= 2 && aml[i - 1] == '\\' && aml[i - 2] == 0x08);
        if (!is_name)
            continue;
        size_t p = i + 4;
        if (aml[p] != 0x12)  // PackageOp
            continue;
        p++;
        if (p >= len)
            return false;
        p += (size_t)((aml[p] >> 6) & 3) + 1;  // PkgLength
        p++;                                   // NumElements
        uint8_t a, b;
        if (!s5_element(aml, len, &p, &a) || !s5_element(aml, len, &p, &b))
            return false;
        *slp_typa = a;
        *slp_typb = b;
        return true;
    }
    return false;
}
