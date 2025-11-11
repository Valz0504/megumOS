#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../header/text/framebuffer.h"
#include "../header/stdlib/string.h"
#include "../header/cpu/portio.h"

static uint8_t cursor_row = 0;
static uint8_t cursor_col = 0;

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    // TODO : Implement
    cursor_row = r;
    cursor_col = c;
    uint16_t pos = r * FRAMEBUFFER_WIDTH + c;

    out(CURSOR_PORT_CMD, 0x0F);
    out(CURSOR_PORT_DATA, (uint8_t) (pos & 0xFF));
    out(CURSOR_PORT_CMD, 0x0E);
    out(CURSOR_PORT_DATA, (uint8_t) ((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    // TODO : Implement
    uint16_t attrib = (bg << 4) | (fg & 0x0F);
    volatile uint16_t * where;
    where = (volatile uint16_t *)FRAMEBUFFER_MEMORY_OFFSET + (row * FRAMEBUFFER_WIDTH + col);
    *where = c | (attrib << 8);
}

void framebuffer_clear(void) {
    // TODO : Implement
    memset(FRAMEBUFFER_MEMORY_OFFSET, 0x00, 80 * 25 * 2);
    for (size_t i = 1; i < (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 2); i += 2) {
        *(FRAMEBUFFER_MEMORY_OFFSET + i) = 0x07;
    }
    cursor_row = 0;
    cursor_col = 0;
    framebuffer_set_cursor(0, 0);
}

void enable_cursor(void) {
    out(CURSOR_PORT_CMD, 0x0A);
    out(CURSOR_PORT_DATA, (in(CURSOR_PORT_DATA) & 0xC0) | 0xE);
    out(CURSOR_PORT_CMD, 0x0B);
    out(CURSOR_PORT_DATA, (in(CURSOR_PORT_DATA) & 0xE0) | 0xF);
}

void putchar(char c, uint8_t color) {
    uint8_t fg = color & 0x0F;
    uint8_t bg = (color >> 4) & 0x0F;
    
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
        framebuffer_set_cursor(cursor_row, cursor_col);
    } else {
        framebuffer_write(cursor_row, cursor_col, c, fg, bg);
        cursor_col++;
        if (cursor_col >= FRAMEBUFFER_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
        framebuffer_set_cursor(cursor_row, cursor_col);
    }
}

void puts(char *str, uint32_t char_count, uint8_t color) {
    for (uint32_t i = 0; i < char_count; i++) {
        putchar(str[i], color);
    }
}
