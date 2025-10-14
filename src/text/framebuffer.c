#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../header/text/framebuffer.h"
#include "../header/stdlib/string.h"
#include "../header/cpu/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    // TODO : Implement
    uint16_t pos = c * 80 + r;

    out(0x3D4, 0x0F);
    out(0x3D5, (uint8_t) (pos & 0xFF));
    out(0x3D4, 0x0E);
    out(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    // TODO : Implement
    uint16_t attrib = (bg << 4) | (fg & 0x0F);
    volatile uint16_t * where;
    where = (volatile uint16_t *)0xB8000 + (row * 80 + col);
    *where = c | (attrib << 8);
}

void framebuffer_clear(void) {
    // TODO : Implement
    memset(FRAMEBUFFER_MEMORY_OFFSET, 0x00, 80 * 25 * 2);
    for (size_t i = 1; i < (80 * 25 * 2); i += 2) {
        *(FRAMEBUFFER_MEMORY_OFFSET + i) = 0x07;
    }
}
