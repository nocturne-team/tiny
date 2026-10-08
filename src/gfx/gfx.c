#include "gfx/gfx.h"
#include "gfx/braille.h"
#include "gfx/font.h"
#include "limine.h"

struct cell {
    uint8_t ch, attr;
    uint8_t braille;  // ch is a braille dot pattern, not a font index
};

static struct cell back[GFX_MAX_ROWS][GFX_MAX_COLS];   // what the UI wants on screen
static struct cell front[GFX_MAX_ROWS][GFX_MAX_COLS];  // what is on screen now
static struct gfx_layout lay;
static volatile uint8_t *fb_base;
static uint64_t fb_pitch;
static uint32_t palette[16];
static bool ready;
static int cur_x = -1, cur_y = -1;  // mouse pointer cell, x < 0 = hidden

static const uint8_t vga_rgb[16][3] = {
    {0, 0, 0},     {0, 0, 170},    {0, 170, 0},    {0, 170, 170},
    {170, 0, 0},   {170, 0, 170},  {170, 85, 0},   {170, 170, 170},
    {85, 85, 85},  {85, 85, 255},  {85, 255, 85},  {85, 255, 255},
    {255, 85, 85}, {255, 85, 255}, {255, 255, 85}, {255, 255, 255},
};

static uint32_t channel(uint8_t v, uint8_t size, uint8_t shift) {
    uint32_t max = (1u << size) - 1;
    return (uint32_t)(v * max / 255) << shift;
}

bool gfx_init(const struct limine_framebuffer *fb) {
    if (fb->bpp != 32 || fb->memory_model != LIMINE_FRAMEBUFFER_RGB)
        return false;
    if (!gfx_compute_layout(fb->width, fb->height, &lay))
        return false;

    fb_base = fb->address;
    fb_pitch = fb->pitch;
    for (int i = 0; i < 16; i++)
        palette[i] = channel(vga_rgb[i][0], fb->red_mask_size, fb->red_mask_shift) |
                     channel(vga_rgb[i][1], fb->green_mask_size, fb->green_mask_shift) |
                     channel(vga_rgb[i][2], fb->blue_mask_size, fb->blue_mask_shift);

    for (uint64_t y = 0; y < fb->height; y++) {
        volatile uint32_t *row = (volatile uint32_t *)(fb_base + y * fb_pitch);
        for (uint64_t x = 0; x < fb->width; x++)
            row[x] = palette[C_BLACK];
    }
    for (int y = 0; y < lay.rows; y++)
        for (int x = 0; x < lay.cols; x++) {
            back[y][x] = (struct cell){ ' ', ATTR(C_LGRAY, C_BLACK), 0 };
            front[y][x] = (struct cell){ 0, 0, 0 };  // differs from back: first flush draws all
        }
    ready = true;
    return true;
}

bool gfx_ready(void) { return ready; }
int gfx_cols(void) { return lay.cols; }
int gfx_rows(void) { return lay.rows; }
const struct gfx_layout *gfx_get_layout(void) { return &lay; }

void gfx_put(int x, int y, uint8_t ch, uint8_t attr) {
    if (x < 0 || y < 0 || x >= lay.cols || y >= lay.rows)
        return;
    back[y][x] = (struct cell){ ch, attr, 0 };
}

void gfx_put_braille(int x, int y, uint8_t pattern, uint8_t attr) {
    if (x < 0 || y < 0 || x >= lay.cols || y >= lay.rows)
        return;
    back[y][x] = (struct cell){ pattern, attr, 1 };
}

int gfx_puts(int x, int y, const char *s, uint8_t attr, int maxlen) {
    int n = 0;
    for (; s[n] && n < maxlen && x + n < lay.cols; n++)
        gfx_put(x + n, y, (uint8_t)s[n], attr);
    return n;
}

void gfx_fill(int x, int y, int w, int h, uint8_t ch, uint8_t attr) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            gfx_put(i, j, ch, attr);
}

static void draw_cell(int cx, int cy, struct cell c) {
    uint8_t dots[16];
    const uint8_t *glyph = font8x16[c.ch];
    if (c.braille) {
        braille_glyph(c.ch, dots);
        glyph = dots;
    }
    uint32_t fg = palette[c.attr & 0x0F], bg = palette[c.attr >> 4];
    int s = lay.scale;
    uint64_t px = (uint64_t)lay.x0 + (uint64_t)cx * 8 * s;
    uint64_t py = (uint64_t)lay.y0 + (uint64_t)cy * 16 * s;

    for (int gy = 0; gy < 16; gy++) {
        uint8_t bits = glyph[gy];
        for (int sy = 0; sy < s; sy++) {
            volatile uint32_t *row =
                (volatile uint32_t *)(fb_base + (py + (uint64_t)(gy * s + sy)) * fb_pitch) + px;
            for (int gx = 0; gx < 8; gx++) {
                uint32_t colour = (bits & (0x80 >> gx)) ? fg : bg;
                for (int sx = 0; sx < s; sx++)
                    *row++ = colour;
            }
        }
    }
}

void gfx_flush(void) {
    if (!ready)
        return;
    for (int y = 0; y < lay.rows; y++)
        for (int x = 0; x < lay.cols; x++) {
            struct cell c = back[y][x];
            if (x == cur_x && y == cur_y)
                c.attr = (uint8_t)(c.attr >> 4 | c.attr << 4);  // the pointer: swapped colours
            if (c.ch != front[y][x].ch || c.attr != front[y][x].attr || c.braille != front[y][x].braille) {
                draw_cell(x, y, c);
                front[y][x] = c;
            }
        }
}

void gfx_set_cursor(int x, int y) {
    cur_x = x;
    cur_y = y;
}
