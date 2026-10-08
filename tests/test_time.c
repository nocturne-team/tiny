#include "test.h"
#include "lib/time.h"

#define CHECK_DT(dt, y, mo, d, h, mi, s) do { \
    CHECK_EQ((dt).year, y); CHECK_EQ((dt).month, mo); CHECK_EQ((dt).day, d); \
    CHECK_EQ((dt).hour, h); CHECK_EQ((dt).min, mi); CHECK_EQ((dt).sec, s); \
} while (0)

static void test_unix(void) {
    struct datetime dt;
    unix_to_datetime(0, &dt);
    CHECK_DT(dt, 1970, 1, 1, 0, 0, 0);
    unix_to_datetime(951782400, &dt);
    CHECK_DT(dt, 2000, 2, 29, 0, 0, 0);
    unix_to_datetime(1700000000, &dt);
    CHECK_DT(dt, 2023, 11, 14, 22, 13, 20);
    unix_to_datetime(-1, &dt);
    CHECK_DT(dt, 1969, 12, 31, 23, 59, 59);
    unix_to_datetime(4102444800LL, &dt);
    CHECK_DT(dt, 2100, 1, 1, 0, 0, 0);
}

static void test_rtc(void) {
    struct datetime dt;
    struct rtc_raw bcd24 = { 0x59, 0x30, 0x23, 0x08, 0x10, 0x26, 0x02 };
    rtc_decode(&bcd24, &dt);
    CHECK_DT(dt, 2026, 10, 8, 23, 30, 59);

    struct rtc_raw bin24 = { 59, 30, 23, 8, 10, 26, 0x06 };
    rtc_decode(&bin24, &dt);
    CHECK_DT(dt, 2026, 10, 8, 23, 30, 59);

    struct rtc_raw pm1 = { 0x00, 0x00, 0x81, 0x01, 0x01, 0x26, 0x00 };  // BCD 12h, 1 PM
    rtc_decode(&pm1, &dt);
    CHECK_EQ(dt.hour, 13);
    struct rtc_raw noon = { 0x00, 0x00, 0x92, 0x01, 0x01, 0x26, 0x00 };  // 12 PM
    rtc_decode(&noon, &dt);
    CHECK_EQ(dt.hour, 12);
    struct rtc_raw midnight = { 0x00, 0x00, 0x12, 0x01, 0x01, 0x26, 0x00 };  // 12 AM
    rtc_decode(&midnight, &dt);
    CHECK_EQ(dt.hour, 0);
    struct rtc_raw bin12 = { 0, 0, 0x80 | 11, 1, 1, 26, 0x04 };  // binary 12h, 11 PM
    rtc_decode(&bin12, &dt);
    CHECK_EQ(dt.hour, 23);
    CHECK_EQ(bcd_to_bin(0x59), 59);
}

static void test_uptime(void) {
    char buf[32];
    fmt_uptime(buf, sizeof buf, 0);
    CHECK_STR(buf, "0:00:00");
    fmt_uptime(buf, sizeof buf, 83);
    CHECK_STR(buf, "0:01:23");
    fmt_uptime(buf, sizeof buf, 3599);
    CHECK_STR(buf, "0:59:59");
    fmt_uptime(buf, sizeof buf, 90061);
    CHECK_STR(buf, "1d 1:01:01");
}

int main(void) {
    test_unix();
    test_rtc();
    test_uptime();
    TEST_MAIN_END();
}
