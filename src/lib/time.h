#pragma once
#include <stddef.h>
#include <stdint.h>

struct datetime {
    int year, month, day, hour, min, sec;
};

// Raw CMOS RTC registers as read from ports 0x70/0x71.
struct rtc_raw {
    uint8_t sec, min, hour, day, month, year, reg_b;
};

void unix_to_datetime(int64_t t, struct datetime *out);
uint8_t bcd_to_bin(uint8_t v);
// Handles BCD/binary (reg B bit 2) and 12/24-hour (reg B bit 1) modes.
void rtc_decode(const struct rtc_raw *raw, struct datetime *out);
// "H:MM:SS", or "Nd H:MM:SS" once a day has passed.
void fmt_uptime(char *buf, size_t cap, uint64_t secs);
