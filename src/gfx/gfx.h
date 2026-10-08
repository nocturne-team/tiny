#pragma once
#include <stdbool.h>
#include <stdint.h>

struct limine_framebuffer;

// 16-colour VGA palette indices.
enum {
    C_BLACK, C_BLUE, C_GREEN, C_CYAN, C_RED, C_MAGENTA, C_BROWN, C_LGRAY,
    C_DGRAY, C_LBLUE, C_LGREEN, C_LCYAN, C_LRED, C_LMAGENTA, C_YELLOW, C_WHITE,
};

// Cell attribute: low nibble foreground, high nibble background.
#define ATTR(fg, bg) ((uint8_t)((fg) | ((bg) << 4)))

#define GFX_MAX_COLS 240
#define GFX_MAX_ROWS 135
#define GFX_MIN_COLS 40
#define GFX_MIN_ROWS 12

struct gfx_layout {
    int cols, rows;  // text grid size
    int scale;       // each font pixel is drawn as scale x scale pixels
    int x0, y0;      // pixel offset of the grid (centred on screen)
};

// Pure: picks grid size and scale for a framebuffer resolution.
bool gfx_compute_layout(uint64_t width, uint64_t height, struct gfx_layout *out);

// Returns false unless the framebuffer is 32 bpp RGB and fits a 40x12 grid.
bool gfx_init(const struct limine_framebuffer *fb);
bool gfx_ready(void);
int gfx_cols(void);
int gfx_rows(void);
const struct gfx_layout *gfx_get_layout(void);

// Cell writes go to a back buffer and are clipped to the grid.
void gfx_put(int x, int y, uint8_t ch, uint8_t attr);
// Draws a braille cell (dot pattern = codepoint - U+2800) instead of a font glyph.
void gfx_put_braille(int x, int y, uint8_t pattern, uint8_t attr);
int gfx_puts(int x, int y, const char *s, uint8_t attr, int maxlen);
void gfx_fill(int x, int y, int w, int h, uint8_t ch, uint8_t attr);
// Draws every cell that changed since the last flush.
void gfx_flush(void);
