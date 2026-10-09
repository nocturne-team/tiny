#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A group of pages: a contiguous run of the UI's page array, picked with an F-key.
struct ui_group {
    const char *title;
    uint8_t first, count;  // pages[first .. first + count)
};

// Pure navigation helpers for the two tab rows.

// The group holding `page`, or -1.
int group_of(const struct ui_group *g, size_t ng, size_t page);
// The page group `gi`'s F-key selects: its next page when `cur` is already in it
// (wrapping), else its last-used page (`last[gi]`, or its first page if that is stale).
size_t group_key(const struct ui_group *g, size_t ng, const uint8_t *last, size_t cur, size_t gi);
// Records `page` as its group's last-used page, unless it takes the raw keyboard
// (the key tester): the group's F-key must never reopen onto a page it can't leave by.
void group_remember(const struct ui_group *g, size_t ng, uint8_t *last, size_t page, bool raw);
// The page digit `digit` (1-9) selects in cur's group, or -1.
long group_digit(const struct ui_group *g, size_t ng, size_t cur, int digit);
// ← / →: the page `delta` steps from `cur`, wrapping.
size_t page_step(size_t npages, size_t cur, int delta);
// The label under column x, for labels drawn from column 2 with one blank column
// between them; -1 for the gaps and past the end.
int label_at(const char *const *labels, size_t n, int x);
