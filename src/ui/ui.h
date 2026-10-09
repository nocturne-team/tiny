#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "ui/page.h"

#define UI_MAX_PAGES 10  // F1-F10

// Draws the frame and runs the input loop forever. `count` <= UI_MAX_PAGES.
__attribute__((noreturn)) void ui_run(struct page *pages, size_t count);
