#pragma once

struct acpi_info;

// Reads FADT/DSDT details used by reboot and shutdown. Safe with no ACPI.
void power_init(const struct acpi_info *acpi);
__attribute__((noreturn)) void power_reboot(void);
__attribute__((noreturn)) void power_shutdown(void);
