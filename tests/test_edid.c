#include "test.h"
#include "info/edid.h"
#include <stdint.h>

static uint8_t e[256];
static struct edid_info info;

static void fix_csum(void) {
    uint8_t sum = 0;
    e[127] = 0;
    for (int i = 0; i < 127; i++)
        sum = (uint8_t)(sum + e[i]);
    e[127] = (uint8_t)(0 - sum);
}

static void name_descriptor(int at, const char *text) {
    uint8_t *d = e + at;
    memset(d, 0, 18);
    d[3] = 0xFC;
    memset(d + 5, ' ', 13);
    size_t n = strlen(text);
    memcpy(d + 5, text, n);
    if (n < 13)
        d[5 + n] = '\n';
}

// A Dell 27" panel: 53x30 cm, preferred 1920x1080 @ 60 Hz.
static void build(void) {
    memset(e, 0, sizeof e);
    static const uint8_t header[8] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    memcpy(e, header, 8);
    e[8] = 0x10; e[9] = 0xAC;               // "DEL"
    e[10] = 0xC2; e[11] = 0xA0;             // product 0xA0C2
    e[12] = 0x78; e[13] = 0x56; e[14] = 0x34; e[15] = 0x12;
    e[16] = 12; e[17] = 30;                 // week 12 of 2020
    e[18] = 1; e[19] = 4;                   // EDID 1.4
    e[21] = 53; e[22] = 30;
    uint8_t *t = e + 54;                    // detailed timing: 148.5 MHz, 1920x1080
    t[0] = 0x02; t[1] = 0x3A;
    t[2] = 0x80; t[3] = 0x18; t[4] = 0x71;  // hactive 1920, hblank 280
    t[5] = 0x38; t[6] = 0x2D; t[7] = 0x40;  // vactive 1080, vblank 45
    name_descriptor(72, "DELL U2720Q");
    fix_csum();
}

static void test_valid(void) {
    build();
    edid_parse(e, 128, &info);
    CHECK(info.valid);
    CHECK(info.checksum_ok);
    CHECK_STR(info.manufacturer, "DEL");
    CHECK_EQ(info.product, 0xA0C2);
    CHECK_EQ(info.serial, 0x12345678);
    CHECK_STR(info.name, "DELL U2720Q");
    CHECK_EQ(info.week, 12);
    CHECK_EQ(info.year, 2020);
    CHECK(!info.model_year);
    CHECK_EQ(info.version, 1);
    CHECK_EQ(info.revision, 4);
    CHECK_EQ(info.width_cm, 53);
    CHECK_EQ(info.height_cm, 30);
    CHECK_EQ(info.diagonal_tenths, 240);  // 24.0 inches
    CHECK_EQ(info.pref_width, 1920);
    CHECK_EQ(info.pref_height, 1080);
    CHECK_EQ(info.pref_centihz, 6000);
}

static void test_variants(void) {
    build();
    e[16] = 0xFF;  // week 0xFF: the year is a model year
    fix_csum();
    edid_parse(e, 128, &info);
    CHECK(info.model_year);

    build();
    e[60] ^= 1;    // corrupt without fixing the checksum
    edid_parse(e, 128, &info);
    CHECK(info.valid);
    CHECK(!info.checksum_ok);

    build();
    memset(e + 54, 0, 18);  // no detailed timing, no size
    e[21] = e[22] = 0;
    fix_csum();
    edid_parse(e, 128, &info);
    CHECK_EQ(info.pref_width, 0);
    CHECK_EQ(info.diagonal_tenths, 0);

    build();
    name_descriptor(72, "\x01" "BAD\x7f" "NAME");  // control bytes sanitised
    fix_csum();
    edid_parse(e, 128, &info);
    CHECK_STR(info.name, "?BAD?NAME");

    build();
    memset(e + 72, 0, 18);  // no name descriptor
    fix_csum();
    edid_parse(e, 128, &info);
    CHECK_STR(info.name, "");
}

static void test_invalid(void) {
    build();
    edid_parse(e, 100, &info);  // too short
    CHECK(!info.valid);
    edid_parse(NULL, 0, &info);
    CHECK(!info.valid);
    build();
    e[0] = 0x42;                // bad header
    edid_parse(e, 128, &info);
    CHECK(!info.valid);
}

int main(void) {
    test_valid();
    test_variants();
    test_invalid();
    TEST_MAIN_END();
}
