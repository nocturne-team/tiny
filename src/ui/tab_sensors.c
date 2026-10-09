#include "info/sensors.h"
#include "lib/fmt.h"
#include "ui/tabs.h"

#define GRAPH_CELLS 60  // 2 samples per cell: the last 2 minutes
#define GRAPH_ROWS 4
#define TEMP_LO 20
#define TEMP_HI 100

// Whole degrees for Intel, tenths (truncated) for AMD's 0.125 °C steps.
static void temp(char *buf, size_t cap, int mc, bool tenths) {
    const char *sign = mc < 0 ? "-" : "";
    int a = mc < 0 ? -mc : mc;
    if (tenths)
        ksnprintf(buf, cap, "%s%d.%d", sign, a / 1000, a / 100 % 10);
    else
        ksnprintf(buf, cap, "%s%d", sign, a / 1000);
}

static void ghz(char *buf, size_t cap, uint64_t khz) {
    ksnprintf(buf, cap, "%llu.%02llu GHz", (unsigned long long)(khz / 1000000),
              (unsigned long long)(khz / 10000 % 100));
}

static void graph(struct page *p, const struct hist *h, int lo, int hi) {
    uint8_t cells[GRAPH_ROWS * GRAPH_CELLS];
    graph_render(h, lo, hi, GRAPH_CELLS, GRAPH_ROWS, cells);
    for (int r = 0; r < GRAPH_ROWS; r++)
        page_graph(p, cells + r * GRAPH_CELLS, GRAPH_CELLS);
}

static void temperatures(struct page *p, const struct sensors *s) {
    page_add(p, STYLE_HEAD, "CPU temperature");
    if (s->kind == SENS_NONE) {
        page_add(p, STYLE_WARN, "No temperature sensor tiny can read (virtual machine, or unsupported CPU)");
        return;
    }
    bool tenths = s->kind != SENS_INTEL;
    char now[16], lo[16], hi[16];
    if (s->has_pkg) {
        const char *key = s->kind == SENS_INTEL ? "Package" : "Tctl";
        if (!s->pkg_ok) {
            page_kv(p, key, "no reading");
        } else {
            temp(now, sizeof now, s->pkg_mc, tenths);
            temp(lo, sizeof lo, s->min_mc, tenths);
            temp(hi, sizeof hi, s->max_mc, tenths);
            page_kv(p, key, "%s \xf8" "C (min %s, max %s)", now, lo, hi);
        }
    }
    if (s->has_core) {
        if (!s->core_ok) {
            page_kv(p, "Core 0", "no reading");
        } else {
            temp(now, sizeof now, s->core_mc, tenths);
            if (s->has_pkg)
                page_kv(p, "Core 0", "%s \xf8" "C", now);
            else
                page_kv(p, "Core 0", "%s \xf8" "C (min %d, max %d)", now, s->min_mc / 1000, s->max_mc / 1000);
        }
    }
    if (s->kind == SENS_INTEL)
        page_kv(p, "TjMax", "%u \xf8" "C%s", s->tjmax, s->tjmax_assumed ? " (assumed)" : "");
    if (s->temp.n) {
        graph(p, &s->temp, TEMP_LO, TEMP_HI);
        page_add(p, STYLE_DIM, "%*slast 2 minutes, %d-%d \xf8" "C", PAGE_KEY_WIDTH, "", TEMP_LO, TEMP_HI);
    }
}

static void clock(struct page *p, const struct sensors *s) {
    page_add(p, STYLE_HEAD, "Clock");
    char a[24], b[24];
    if (!s->has_clock)
        page_kv(p, "Effective", "not available (no APERF/MPERF)");
    else if (!s->khz)
        page_kv(p, "Effective", "measuring...");
    else {
        ghz(a, sizeof a, s->khz);
        ghz(b, sizeof b, s->max_khz);
        page_kv(p, "Effective", "%s (max %s)", a, b);
    }
    if (s->tsc_hz) {
        ghz(a, sizeof a, s->tsc_hz / 1000);
        page_kv(p, "Base (TSC)", "%s", a);
    }
    if (s->clock.n) {
        int hi = (int)((s->max_khz / 1000 + 499) / 500 * 500);
        if (hi < 500)
            hi = 500;
        graph(p, &s->clock, 0, hi);
        page_add(p, STYLE_DIM, "%*slast 2 minutes, 0-%d.%d GHz", PAGE_KEY_WIDTH, "", hi / 1000, hi / 100 % 10);
    }
}

void tab_sensors_build(struct page *p, const struct sensors *s) {
    page_clear(p);
    p->title = "Sensors";
    temperatures(p, s);
    page_blank(p);
    clock(p, s);
    page_blank(p);
    page_add(p, STYLE_DIM, "Readings are from the boot CPU only. tiny keeps that core busy, so it shows");
    page_add(p, STYLE_DIM, "the clock and temperature of one fully loaded core.");
}
