#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h" // <--- Mengimpor out(port, data)

#define FB_ROWS 25
#define FB_COLS 80

static uint16_t *const FRAMEBUFFER = (uint16_t *)FRAMEBUFFER_MEMORY_OFFSET;

void framebuffer_set_cursor(uint8_t r, uint8_t c)
{
    uint16_t pos = r * FB_COLS + c;

    // Mengirimkan high byte
    out(CURSOR_PORT_CMD, 0x0E);
    out(CURSOR_PORT_DATA, (uint8_t)((pos >> 8) & 0xFF));

    // Mengirimkan low byte
    out(CURSOR_PORT_CMD, 0x0F);
    out(CURSOR_PORT_DATA, (uint8_t)(pos & 0xFF));
}

/**
 * Menulis karakter dan warna ke lokasi framebuffer tertentu.
 * Setiap sel memori 16-bit (uint16_t) di framebuffer terdiri dari:
 * - Byte Rendah (Bits 0-7): Karakter ASCII
 * - Byte Tinggi (Bits 8-15): Warna Atribut (BG di atas, FG di bawah)
 * - Bits 12-15: Background color (bg)
 * - Bits 8-11: Character/Foreground color (fg)
 */
void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg)
{
    uint16_t offset = row * FB_COLS + col;

    uint8_t attribute_byte = (bg << 4) | (fg & 0x0F);

    uint16_t entry = (attribute_byte << 8) | c;

    FRAMEBUFFER[offset] = entry;
}

void framebuffer_clear(void)
{
    // Warna default: Gray (0x07) foreground, Black (0x00) background
    const uint16_t default_entry = 0x0700;

    // Total sel: 80 * 25 = 2000
    const size_t total_cells = FB_ROWS * FB_COLS;

    // Mengisi seluruh area framebuffer dengan nilai default
    for (size_t i = 0; i < total_cells; ++i)
    {
        FRAMEBUFFER[i] = default_entry;
    }

    framebuffer_set_cursor(0, 0);
}