#pragma once
#include <stdbool.h>
#include <stddef.h>

#define POINTER_COUNTS_X 6   // mouse counts per text column
#define POINTER_COUNTS_Y 12  // mouse counts per text row
#define POINTER_WHEEL_LINES 3

// Mouse pointer on the text grid.
struct pointer {
    int x, y;    // cell
    int fx, fy;  // leftover counts toward the next cell, |f| < COUNTS
    bool visible;
};

// Centred and hidden.
void pointer_init(struct pointer *p, int cols, int rows);
// Applies relative motion, clamped to the grid, and shows the pointer.
// True when its cell changed or it just became visible.
bool pointer_move(struct pointer *p, int dx, int dy, int cols, int rows);
// Lines to scroll for a wheel delta (wheel up = towards the top = negative).
long pointer_wheel_lines(int wheel);
