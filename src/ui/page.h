#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_LINE_LEN 120   // bytes per line, including the NUL
#define PAGE_MAX_LINES 300
#define PAGE_KEY_WIDTH 16   // page_kv key column: 15 chars + 1 space

// STYLE_ART lines hold one byte: the 1-based row of the About art to draw.
enum line_style { STYLE_TEXT, STYLE_HEAD, STYLE_DIM, STYLE_WARN, STYLE_ART };

struct line {
    uint8_t style;
    char text[PAGE_LINE_LEN];
};

struct page {
    const char *title;
    size_t count;
    size_t scroll;    // index of the first visible line
    bool truncated;   // lines were dropped; the last line says so
    // Optional, for live pages: rebuilds the page when its data changed and
    // returns true then. The UI keeps the scroll position across rebuilds.
    bool (*update)(struct page *p);
    bool follow;      // opens at the end and stays there while scrolled to the end
    struct line lines[PAGE_MAX_LINES];
};

void page_clear(struct page *p);
void page_blank(struct page *p);
void page_add(struct page *p, enum line_style style, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
// "Key            value": key cut to 15 chars, value starts at PAGE_KEY_WIDTH.
void page_kv(struct page *p, const char *key, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
// Lays words out in the value column, wrapping at `width` total columns.
void page_wrap_words(struct page *p, const char *key, const char *const *words, size_t n, size_t width);
void page_scroll(struct page *p, long delta, size_t visible);
void page_scroll_to(struct page *p, bool end, size_t visible);
// True when the last line is on screen.
bool page_at_end(const struct page *p, size_t visible);
