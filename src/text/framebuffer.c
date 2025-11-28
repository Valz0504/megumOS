#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../header/text/framebuffer.h"
#include "../header/stdlib/string.h"
#include "../header/cpu/portio.h"

static uint8_t cursor_row = 0;
static uint8_t cursor_col = 0;
static void framebuffer_scroll(void);

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

void puts(char* str, uint32_t len, uint8_t color) {
    for (uint32_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '\n') {
            cursor_row++;
            cursor_col = 0;
        } else if (c == '\b') {
            if (cursor_col > 0) {
                cursor_col--;
                framebuffer_write(cursor_row, cursor_col, ' ', 0x0F, 0x00);
            }
        } else if (c == '\t') {
            for (int j = 0; j < 4; j++) {
                framebuffer_write(cursor_row, cursor_col, ' ', color, 0x0);
                if (++cursor_col >= FRAMEBUFFER_WIDTH) {
                    cursor_col = 0;
                    cursor_row++;
                }
            }
        } else {
            framebuffer_write(cursor_row, cursor_col, c, color, 0x0);
            cursor_col++;
            if (cursor_col >= FRAMEBUFFER_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
        }
        if (cursor_row >= FRAMEBUFFER_HEIGHT) {
            framebuffer_scroll();
        }
    }
    framebuffer_set_cursor(cursor_row, cursor_col);
}

void putchar(char c, uint8_t color) {
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            framebuffer_write(cursor_row, cursor_col, ' ', 0x0F, 0x00);
        }
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            framebuffer_write(cursor_row, cursor_col, ' ', color, 0x0);
            if (++cursor_col >= FRAMEBUFFER_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
        }
    } else {
        framebuffer_write(cursor_row, cursor_col, c, color, 0x0);
        cursor_col++;
        if (cursor_col >= FRAMEBUFFER_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= FRAMEBUFFER_HEIGHT) {
        framebuffer_scroll();
    }
    framebuffer_set_cursor(cursor_row, cursor_col);
}

// Scroll framebuffer up by one row and keep cursor at last row
static void framebuffer_scroll(void) {
    // Move all rows up by one
    size_t row_bytes = FRAMEBUFFER_WIDTH * 2;
    size_t total_bytes = FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 2;
    char *fb = (char *)FRAMEBUFFER_MEMORY_OFFSET;
    memmove(fb, fb + row_bytes, total_bytes - row_bytes);

    // Clear last row
    for (size_t i = total_bytes - row_bytes; i < total_bytes; i += 2) {
        fb[i] = ' ';
        fb[i + 1] = 0x07;
    }

    if (cursor_row > 0) cursor_row = FRAMEBUFFER_HEIGHT - 1;
    cursor_col = 0;
}
