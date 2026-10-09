#include "test.h"
#include "ui/screen_test.h"

static struct raw_key key(uint8_t usage, bool down) {
    struct raw_key k = { 0 };
    k.src = RAW_USB;
    k.down = down;
    k.usage = usage;
    return k;
}

int main(void) {
    // Names and solids.
    CHECK_STR(screen_pattern_name(0), "Test card");
    CHECK_STR(screen_pattern_name(1), "Black");
    CHECK_STR(screen_pattern_name(SCREEN_PATTERNS - 1), "Grid 32 px, red edge");
    CHECK_STR(screen_pattern_name(SCREEN_PATTERNS), "?");
    CHECK(!screen_pattern_solid(0));  // test card
    CHECK(screen_pattern_solid(1));
    CHECK(screen_pattern_solid(6));
    CHECK(!screen_pattern_solid(7));
    CHECK(!screen_pattern_solid(-1));

    // Solid colours.
    CHECK_EQ(screen_pixel(1, 5, 5, 640, 480), 0x000000);
    CHECK_EQ(screen_pixel(2, 5, 5, 640, 480), 0xFFFFFF);
    CHECK_EQ(screen_pixel(3, 5, 5, 640, 480), 0xFF0000);
    CHECK_EQ(screen_pixel(4, 5, 5, 640, 480), 0x00FF00);
    CHECK_EQ(screen_pixel(5, 5, 5, 640, 480), 0x0000FF);
    CHECK_EQ(screen_pixel(6, 5, 5, 640, 480), 0x808080);

    // Gradients: four bands top to bottom (red, green, blue, grey), 0 to 255 left to right.
    CHECK_EQ(screen_pixel(7, 0, 0, 640, 480), 0x000000);
    CHECK_EQ(screen_pixel(7, 639, 0, 640, 480), 0xFF0000);
    CHECK_EQ(screen_pixel(7, 639, 120, 640, 480), 0x00FF00);
    CHECK_EQ(screen_pixel(7, 639, 240, 640, 480), 0x0000FF);
    CHECK_EQ(screen_pixel(7, 639, 479, 640, 480), 0xFFFFFF);
    CHECK_EQ(screen_pixel(7, 256, 479, 513, 480), 0x7F7F7F);  // 256 * 255 / 512
    CHECK_EQ(screen_pixel(7, 0, 0, 1, 1), 0x000000);          // one pixel wide: no divide by zero

    // Checkerboard.
    CHECK_EQ(screen_pixel(8, 0, 0, 640, 480), 0x000000);
    CHECK_EQ(screen_pixel(8, 1, 0, 640, 480), 0xFFFFFF);
    CHECK_EQ(screen_pixel(8, 1, 1, 640, 480), 0x000000);

    // Grid: red outermost pixels, white lines every 32 px, black between.
    CHECK_EQ(screen_pixel(9, 0, 100, 640, 480), 0xFF0000);
    CHECK_EQ(screen_pixel(9, 639, 100, 640, 480), 0xFF0000);
    CHECK_EQ(screen_pixel(9, 100, 479, 640, 480), 0xFF0000);
    CHECK_EQ(screen_pixel(9, 32, 100, 640, 480), 0xFFFFFF);
    CHECK_EQ(screen_pixel(9, 100, 64, 640, 480), 0xFFFFFF);
    CHECK_EQ(screen_pixel(9, 33, 65, 640, 480), 0x000000);

    // Test card on 1280 x 720: castellated edge, grid, circle with bars, wedge, gratings.
    // Centre (640, 360), circle radius 300, castellation blocks 36 px, grid every 60 px from the centre.
    CHECK_EQ(screen_pixel(0, 0, 0, 1280, 720), 0x000000);       // castellation: corner block black
    CHECK_EQ(screen_pixel(0, 36, 0, 1280, 720), 0xFFFFFF);      // next block white
    CHECK_EQ(screen_pixel(0, 1279, 719, 1280, 720), 0x000000);  // opposite corner, same parity
    CHECK_EQ(screen_pixel(0, 110, 110, 1280, 720), 0x7F7F7F);   // grey background
    CHECK_EQ(screen_pixel(0, 100, 360, 1280, 720), 0xFFFFFF);   // grid line through the centre row
    CHECK_EQ(screen_pixel(0, 640, 60, 1280, 720), 0xFFFFFF);    // circle outline at the top
    CHECK_EQ(screen_pixel(0, 375, 230, 1280, 720), 0xFFFFFF);   // colour bars: first bar white...
    CHECK_EQ(screen_pixel(0, 640 - 37, 200, 1280, 720), 0x00FF00);  // ...fourth green
    CHECK_EQ(screen_pixel(0, 640 + 37, 200, 1280, 720), 0xFF00FF);  // ...fifth magenta
    CHECK_EQ(screen_pixel(0, 640, 300, 1280, 720), 0xFFFFFF);   // crosshair
    CHECK_EQ(screen_pixel(0, 600, 300, 1280, 720), 0x202020);   // dark centre
    CHECK_EQ(screen_pixel(0, 380, 430, 1280, 720), 0x000000);   // grey wedge: first step black
    CHECK_EQ(screen_pixel(0, 900, 430, 1280, 720), 0xFFFFFF);   // ...last step white
    CHECK_EQ(screen_pixel(0, 900, 500, 1280, 720), 0xFFFFFF);   // last grating: 1 px stripes
    CHECK_EQ(screen_pixel(0, 901, 500, 1280, 720), 0x000000);
    CHECK_EQ(screen_pixel(0, 600, 620, 1280, 720), 0x000000);   // bottom: black half...
    CHECK_EQ(screen_pixel(0, 680, 620, 1280, 720), 0xFFFFFF);   // ...white half
    CHECK_EQ(screen_pixel(0, 0, 0, 1, 1), 0x000000);            // tiny screens don't crash
    CHECK_EQ(screen_pixel(0, 3, 3, 8, 8), screen_pixel(0, 3, 3, 8, 8));

    // Keys: presses only; Enter ignored.
    struct raw_key k;
    k = key(0x2C, true); CHECK_EQ(screen_key(&k), SCREEN_NEXT);   // Space
    k = key(0x4F, true); CHECK_EQ(screen_key(&k), SCREEN_NEXT);   // Right
    k = key(0x4E, true); CHECK_EQ(screen_key(&k), SCREEN_NEXT);   // PgDn
    k = key(0x50, true); CHECK_EQ(screen_key(&k), SCREEN_PREV);   // Left
    k = key(0x2A, true); CHECK_EQ(screen_key(&k), SCREEN_PREV);   // Backspace
    k = key(0x4B, true); CHECK_EQ(screen_key(&k), SCREEN_PREV);   // PgUp
    k = key(0x29, true); CHECK_EQ(screen_key(&k), SCREEN_LEAVE);  // Esc
    k = key(0x14, true); CHECK_EQ(screen_key(&k), SCREEN_LEAVE);  // Q
    k = key(0x28, true); CHECK_EQ(screen_key(&k), SCREEN_NONE);   // Enter
    k = key(0x2C, false); CHECK_EQ(screen_key(&k), SCREEN_NONE);  // Space release
    k = key(0x2C, true); k.flags = RAW_REPEAT; CHECK_EQ(screen_key(&k), SCREEN_NONE);

    // Stepping.
    CHECK_EQ(screen_step(0, SCREEN_NEXT), 1);
    CHECK_EQ(screen_step(0, SCREEN_PREV), 0);
    CHECK_EQ(screen_step(3, SCREEN_PREV), 2);
    CHECK_EQ(screen_step(SCREEN_PATTERNS - 1, SCREEN_NEXT), -1);
    CHECK_EQ(SCREEN_PATTERNS, 10);
    CHECK_EQ(screen_step(4, SCREEN_LEAVE), -1);
    CHECK_EQ(screen_step(4, SCREEN_NONE), 4);

    // Native mode.
    CHECK_EQ(screen_native(1920, 1080, 1920, 1080), SCREEN_NATIVE_YES);
    CHECK_EQ(screen_native(1024, 768, 1920, 1080), SCREEN_NATIVE_NO);
    CHECK_EQ(screen_native(1024, 768, 0, 0), SCREEN_NATIVE_UNKNOWN);
    TEST_MAIN_END();
}
