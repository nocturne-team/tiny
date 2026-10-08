#pragma once
#include <stdbool.h>
#include <stdint.h>

struct smbios_info {
    bool present;
    bool is64;         // data came from the 64-bit (SMBIOS 3) entry point
    bool checksum_ok;  // entry point checksum
    unsigned major, minor;
    char bios_vendor[48], bios_version[48], bios_date[16];    // type 0
    char sys_vendor[48], sys_product[48], sys_version[48];    // type 1
    char board_vendor[48], board_product[48];                 // type 2
};

// entry32/entry64 are virtual pointers to the entry points (either may be NULL).
// The structure table address inside is physical and converted with `hhdm`.
void smbios_parse(const void *entry32, const void *entry64, uint64_t hhdm, struct smbios_info *out);
