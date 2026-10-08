#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lib/time.h"

// Reads the CMOS clock. Returns false if no RTC responds. reg_b may be NULL.
bool rtc_read(struct datetime *out, uint8_t *reg_b);
