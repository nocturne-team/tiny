#include "info/rtc.h"
#include "arch/x86.h"
#include "lib/string.h"

static uint8_t cmos(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static bool updating(void) {
    return cmos(0x0A) & 0x80;
}

static bool read_raw(struct rtc_raw *r) {
    for (int i = 0; i < 100000 && updating(); i++)
        ;
    if (updating())
        return false;
    r->sec = cmos(0x00);
    r->min = cmos(0x02);
    r->hour = cmos(0x04);
    r->day = cmos(0x07);
    r->month = cmos(0x08);
    r->year = cmos(0x09);
    r->reg_b = cmos(0x0B);
    return true;
}

bool rtc_read(struct datetime *out, uint8_t *reg_b) {
    struct rtc_raw a, b;
    // Read until two consecutive reads agree, so we never see a half-updated clock.
    for (int tries = 0; tries < 5; tries++) {
        if (!read_raw(&a) || !read_raw(&b))
            return false;
        if (memcmp(&a, &b, sizeof a) == 0) {
            if (a.month == 0xFF && a.day == 0xFF)
                return false;  // floating bus: no CMOS
            rtc_decode(&a, out);
            if (reg_b)
                *reg_b = a.reg_b;
            return true;
        }
    }
    return false;
}
