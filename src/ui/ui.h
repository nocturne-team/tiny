#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "ui/page.h"

#include "ui/groups.h"

#define UI_MAX_PAGES 16
#define UI_MAX_GROUPS 9  // F1-F9

// Whether R R / S S show the farewell first (off with the "nofarewell" command line).
void ui_set_farewell(bool on);
// Draws the frame and runs the input loop forever. Pages are grouped by `groups`
// (contiguous runs of `pages`, in order); counts above the limits are cut.
__attribute__((noreturn)) void ui_run(struct page *pages, size_t count, const struct ui_group *groups,
                                      size_t ngroups);
