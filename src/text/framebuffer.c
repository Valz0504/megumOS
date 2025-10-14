#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h" 

#define FB_ROWS 25
#define FB_COLS 80

static uint16_t *const FRAMEBUFFER = (uint16_t *)FRAMEBUFFER_MEMORY_OFFSET;

void framebuffer_set_cursor(uint8_t r, uint8_t c)
{
    uint16_t pos = r * FB_COLS + c;
    out(CURSOR_PORT_CMD, 0x0E);
    out(CURSOR_PORT_DATA, (uint8_t)((pos >> 8) & 0xFF));
    out(CURSOR_PORT_CMD, 0x0F);
    out(CURSOR_PORT_DATA, (uint8_t)(pos & 0xFF));
}


void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg)
{
    uint16_t offset = row * FB_COLS + col;
    uint8_t attribute_byte = (bg << 4) | (fg & 0x0F);
    uint16_t entry = (attribute_byte << 8) | c;
    FRAMEBUFFER[offset] = entry;
}

void framebuffer_clear(void)
{
    const uint16_t default_entry = 0x0700;
    const size_t total_cells = FB_ROWS * FB_COLS;
    for (size_t i = 0; i < total_cells; ++i)
    {
        FRAMEBUFFER[i] = default_entry;
    }
    framebuffer_set_cursor(0, 0);
}