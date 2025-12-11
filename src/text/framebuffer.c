#include "../header/text/framebuffer.h"
#include "../header/stdlib/string.h"
#include "../header/graphics/graphics.h"

static uint8_t cursor_row = 0;
static uint8_t cursor_col = 0;
// Graphics text grid size (320x200, block 5x8) -> 64x25
#define FB_COLS (SCREEN_WIDTH / BLOCK_WIDTH)
#define FB_ROWS (SCREEN_HEIGHT / BLOCK_HEIGHT)

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    if (r >= FB_ROWS) r = FB_ROWS - 1;
    if (c >= FB_COLS) c = FB_COLS - 1;
    cursor_row = r;
    cursor_col = c;
    graphics_set_cursor(c, r);
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    (void) fg;
    (void) bg;
    if (row >= FB_ROWS || col >= FB_COLS) return;
    graphics_write_char_default(col, row, c);
    swap_buffer();
}

void framebuffer_clear(void) {
    clear_screen();
    cursor_row = 0;
    cursor_col = 0;
    graphics_set_cursor(0, 0);
    swap_buffer();
}

void framebuffer_clear_color(uint8_t color) {
    clear_screen_color(color);
    cursor_row = 0;
    cursor_col = 0;
    graphics_set_cursor(0, 0);
    swap_buffer();
}

void framebuffer_draw_text_frame_color(const char *buf, uint8_t width, uint8_t height, uint8_t color) {
    if (buf == NULL) return;
    if (width > FB_COLS) width = FB_COLS;
    if (height > FB_ROWS) height = FB_ROWS;

    // Draw provided frame
    for (uint8_t y = 0; y < height; ++y) {
        const char *row = buf + (uint32_t)y * width;
        uint8_t x = 0;
        for (; x < width; ++x) {
            graphics_write_char(x, y, row[x], color);
        }
        for (; x < FB_COLS; ++x) {
            graphics_write_char(x, y, ' ', color);
        }
    }

    // Clear remaining rows if the frame height is shorter than the screen
    for (uint8_t y = height; y < FB_ROWS; ++y) {
        for (uint8_t x = 0; x < FB_COLS; ++x) {
            graphics_write_char(x, y, ' ', color);
        }
    }

    cursor_row = 0;
    cursor_col = 0;
    graphics_set_cursor(0, 0);
    swap_buffer();
}

void framebuffer_draw_text_frame(const char *buf, uint8_t width, uint8_t height) {
    framebuffer_draw_text_frame_color(buf, width, height, DEFAULT_COLOR_FG);
}

void enable_cursor(void) {
    draw_cursor();
    swap_buffer();
}

void puts(char* str, uint32_t len, uint8_t color) {
    (void) color;
    for (uint32_t i = 0; i < len; i++) {
        graphics_putchar(str[i]);
    }
}

void putchar(char c, uint8_t color) {
    (void) color;
    graphics_putchar(c);
}
