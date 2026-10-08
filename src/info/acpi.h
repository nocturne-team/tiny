#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lib/string.h"

#define ACPI_MAX_TABLES 64
#define ACPI_MAX_LEN 0x1000000  // lengths above 16 MiB are treated as corrupt

struct acpi_table {
    char sig[5];
    char oem_id[7];
    uint32_t length;
    bool checksum_ok;  // also false when the length is implausible
    const void *ptr;   // virtual address of the table header
};

struct acpi_info {
    bool present;
    uint8_t revision;  // RSDP revision: 0 = ACPI 1.0, >= 2 = ACPI 2.0+
    char oem_id[7];
    bool rsdp_checksum_ok;
    bool use_xsdt;
    bool truncated;    // more than ACPI_MAX_TABLES tables
    size_t table_count;
    struct acpi_table tables[ACPI_MAX_TABLES];
};

static inline uint32_t acpi_rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t acpi_rd64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }

bool acpi_checksum(const void *p, size_t len);
// Walks RSDP -> RSDT/XSDT -> tables (+ DSDT via the FADT). `rsdp` is a virtual
// pointer; physical addresses inside the tables are converted with `hhdm`.
void acpi_parse(const void *rsdp, uint64_t hhdm, struct acpi_info *out);
const struct acpi_table *acpi_find(const struct acpi_info *info, const char *sig);
// Byte-scans AML for Name(\_S5_, Package(){a, b, ...}) and returns SLP_TYPa/b.
bool acpi_find_s5(const uint8_t *aml, size_t len, uint8_t *slp_typa, uint8_t *slp_typb);
