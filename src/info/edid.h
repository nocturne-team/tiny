#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct edid_info {
    bool valid;        // at least 128 bytes with the fixed 8-byte header
    bool checksum_ok;
    char manufacturer[4];  // 3-letter PNP ID, e.g. "DEL"
    uint16_t product;
    uint32_t serial;
    char name[14];         // monitor name descriptor (0xFC), "" if absent
    int week, year;        // week 0 = unknown
    bool model_year;       // week was 0xFF: `year` is the model year
    uint8_t version, revision;
    uint8_t width_cm, height_cm;   // 0 = unknown / projector
    unsigned diagonal_tenths;      // inches x 10, 0 if size unknown
    uint16_t pref_width, pref_height;  // first detailed timing, 0 if none
    uint32_t pref_centihz;             // refresh rate x 100
};

// Pure: decodes an EDID base block. Never reads past `len`.
void edid_parse(const uint8_t *edid, size_t len, struct edid_info *out);
