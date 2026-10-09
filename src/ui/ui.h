#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "ui/page.h"

#define UI_MAX_PAGES 11  // F1-F11

// Whether R R / S S show the farewell first (off with the "nofarewell" command line).
void ui_set_farewell(bool on);
// Draws the frame and runs the input loop forever. `count` <= UI_MAX_PAGES.
__attribute__((noreturn)) void ui_run(struct page *pages, size_t count);
