#include "ui/keys_state.h"
#include "lib/fmt.h"
#include "lib/string.h"
#include "ui/kbd_layout.h"

#define USAGE_ESC 0x29
#define USAGE_ENTER 0x28
#define USAGE_KP_ENTER 0x58

static bool get(const uint8_t *set, uint8_t u) {
    return set[u >> 3] & (1u << (u & 7));
}

static void put(uint8_t *set, uint8_t u, bool on) {
    if (on)
        set[u >> 3] |= (uint8_t)(1u << (u & 7));
    else
        set[u >> 3] &= (uint8_t)~(1u << (u & 7));
}

bool keys_is_held(const struct keys_state *s, uint8_t usage) {
    return get(s->held, usage);
}

bool keys_is_tested(const struct keys_state *s, uint8_t usage) {
    return get(s->tested, usage);
}

static void add_line(struct keys_state *s, const struct raw_key *ev) {
    struct keys_line *top = &s->lines[0];
    if ((ev->flags & RAW_REPEAT) && s->nlines && top->ev.down && top->ev.src == ev->src &&
        top->ev.len == ev->len && memcmp(top->ev.bytes, ev->bytes, ev->len) == 0) {
        top->count++;
        return;
    }
    memmove(&s->lines[1], &s->lines[0], sizeof s->lines[0] * (KEYS_LINES - 1));
    s->lines[0].ev = *ev;
    s->lines[0].count = 1;
    if (s->nlines < KEYS_LINES)
        s->nlines++;
}

bool keys_apply(struct keys_state *s, const struct raw_key *ev, uint64_t now) {
    if (s->skip_enter && (ev->usage == USAGE_ENTER || ev->usage == USAGE_KP_ENTER)) {
        if (!ev->down)
            s->skip_enter = false;
        return false;
    }
    add_line(s, ev);
    uint8_t u = ev->usage;
    if (!u)
        return false;  // rollover error or unmapped: nothing to light up
    if (!ev->down) {
        if (get(s->held, u)) {
            put(s->held, u, false);
            s->held_n--;
        }
        if (u == USAGE_ESC)
            s->esc_down = false;
        return false;
    }
    if (!get(s->tested, u)) {
        put(s->tested, u, true);
        s->tested_n++;
    }
    if (u == USAGE_ESC && !s->esc_down) {
        s->esc_down = true;
        s->esc_since = now;
    }
    if (get(s->held, u))
        return false;  // typematic repeat
    put(s->held, u, true);
    if (++s->held_n > s->max_held)
        s->max_held = s->held_n;
    uint8_t flip = u == 0x53 ? 1 : u == 0x39 ? 2 : u == 0x47 ? 4 : 0;  // Num, Caps, Scroll
    if (!flip)
        return false;
    s->leds ^= flip;
    return true;
}

bool keys_esc_held(const struct keys_state *s, uint64_t now) {
    return s->esc_down && now - s->esc_since >= KEYS_HOLD_MS;
}

void keys_reset_held(struct keys_state *s) {
    memset(s->held, 0, sizeof s->held);
    s->held_n = 0;
    s->esc_down = false;
}

void keys_start(struct keys_state *s) {
    keys_reset_held(s);
    s->skip_enter = true;
}

unsigned keys_untested(const struct keys_state *s) {
    unsigned n = 0;
    for (int i = 0; i < KBD_KEYS; i++)
        n += !get(s->tested, kbd_keys[i].usage);
    return n;
}

void keys_line_text(const struct keys_line *l, char *buf, size_t cap) {
    const struct raw_key *e = &l->ev;
    char src[32];
    if (e->src == RAW_USB) {
        ksnprintf(src, sizeof src, "USB %s", e->port);
    } else {
        size_t n = (size_t)ksnprintf(src, sizeof src, "PS/2");
        for (size_t i = 0; i < e->len && n + 4 < sizeof src; i++)
            n += (size_t)ksnprintf(src + n, sizeof src - n, " %02X", e->bytes[i]);
    }
    if (e->flags & RAW_ROLLOVER) {
        ksnprintf(buf, cap, "%-23s  rollover error (too many keys)", src);
        return;
    }
    const char *act = e->src == RAW_USB ? (e->down ? "press" : "release") : (e->down ? "make" : "break");
    char rest[48];
    if (!e->usage) {
        ksnprintf(rest, sizeof rest, "unmapped");
    } else {
        const char *name = kbd_usage_name(e->usage);
        ksnprintf(rest, sizeof rest, "0x%02x  %s", e->usage, name ? name : "?");
    }
    size_t n = (size_t)ksnprintf(buf, cap, "%-23s  %-7s  %s", src, act, rest);
    if (l->count > 1 && n < cap)
        ksnprintf(buf + n, cap - n, "  x%u", l->count);
}
