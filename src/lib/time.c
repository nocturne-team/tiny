#include "lib/time.h"
#include <stdbool.h>
#include "lib/fmt.h"

// Days-to-civil conversion from Howard Hinnant's date algorithms.
void unix_to_datetime(int64_t t, struct datetime *out) {
    int64_t days = t / 86400, secs = t % 86400;
    if (secs < 0) {
        secs += 86400;
        days--;
    }
    out->hour = (int)(secs / 3600);
    out->min = (int)(secs / 60 % 60);
    out->sec = (int)(secs % 60);

    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint64_t doe = (uint64_t)(z - era * 146097);
    uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint64_t mp = (5 * doy + 2) / 153;
    out->day = (int)(doy - (153 * mp + 2) / 5 + 1);
    out->month = (int)(mp < 10 ? mp + 3 : mp - 9);
    out->year = (int)((int64_t)yoe + era * 400 + (out->month <= 2));
}

uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

void rtc_decode(const struct rtc_raw *r, struct datetime *out) {
    bool bcd = !(r->reg_b & 0x04);
    bool h24 = r->reg_b & 0x02;
    bool pm = r->hour & 0x80;
#define CONV(v) (bcd ? bcd_to_bin(v) : (v))
    int hour = CONV((uint8_t)(r->hour & 0x7F));
    out->sec = CONV(r->sec);
    out->min = CONV(r->min);
    out->day = CONV(r->day);
    out->month = CONV(r->month);
    out->year = 2000 + CONV(r->year);
#undef CONV
    if (!h24)
        hour = hour % 12 + (pm ? 12 : 0);
    out->hour = hour;
}

void fmt_uptime(char *buf, size_t cap, uint64_t secs) {
    unsigned long d = (unsigned long)(secs / 86400);
    unsigned long h = (unsigned long)(secs / 3600 % 24);
    unsigned long m = (unsigned long)(secs / 60 % 60);
    unsigned long s = (unsigned long)(secs % 60);
    if (d)
        ksnprintf(buf, cap, "%lud %lu:%02lu:%02lu", d, h, m, s);
    else
        ksnprintf(buf, cap, "%lu:%02lu:%02lu", h, m, s);
}
