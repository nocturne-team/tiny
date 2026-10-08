#include "gfx/gfx.h"

bool gfx_compute_layout(uint64_t width, uint64_t height, struct gfx_layout *out) {
    uint64_t sw = width / 1280, sh = height / 720;
    uint64_t scale = sw < sh ? sw : sh;
    if (scale < 1)
        scale = 1;

    uint64_t cols = width / (8 * scale), rows = height / (16 * scale);
    if (cols > GFX_MAX_COLS)
        cols = GFX_MAX_COLS;
    if (rows > GFX_MAX_ROWS)
        rows = GFX_MAX_ROWS;
    if (cols < GFX_MIN_COLS || rows < GFX_MIN_ROWS)
        return false;

    out->cols = (int)cols;
    out->rows = (int)rows;
    out->scale = (int)scale;
    out->x0 = (int)((width - cols * 8 * scale) / 2);
    out->y0 = (int)((height - rows * 16 * scale) / 2);
    return true;
}
