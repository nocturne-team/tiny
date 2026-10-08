#pragma once
#include <stdint.h>

enum tsc_source { TSC_SRC_NONE, TSC_SRC_LIMINE, TSC_SRC_CPUID, TSC_SRC_PIT };

// Determines the TSC frequency: Limine, then CPUID 0x15/0x16, then the PIT.
void tsc_init(void);
uint64_t tsc_hz(void);                // 0 when unknown
enum tsc_source tsc_source(void);
uint64_t tsc_uptime_seconds(void);    // since tsc_init; 0 when frequency unknown
