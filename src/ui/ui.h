#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "ui/page.h"

// Draws the frame and runs the keyboard loop forever. `count` <= 5 (F1-F5).
__attribute__((noreturn)) void ui_run(struct page *pages, size_t count, bool have_keyboard);
