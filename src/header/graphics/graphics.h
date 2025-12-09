#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include <stdint.h>

#define MISC_OUT_REG 0x3c2
#define FEAT_CONT_REG 0x3ca

#define ATTR_ADDR_REG 0x3c0
#define ATTR_DATA_REG 0x3c1

#define CRT_ADDR_REG 0x3d4
#define CRT_DATA_REG 0x3d5

#define MEM_MODE_REG 0x3c4

#define DAC_MASK_REG 0x3c6
#define DAC_READ_REG 0x3c7
#define DAC_WRITE_REG 0x3c8
#define DAC_DATA_REG 0x3c9
#define DAC_STATE_REG 0x3c7

#define GRAP_ADDR_REG 0x3ce
#define GRAP_DATA_REG 0x3cf

#define INPUT_STATUS_1 0x3da

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

#define BLOCK_WIDTH 5
#define BLOCK_HEIGHT 8

#define DEFAULT_COLOR_BG 0
#define DEFAULT_COLOR_FG 250
#define CURSOR_COLOR DEFAULT_COLOR_FG

#define MEMORY_GRAPHICS ((uint8_t *)0xC00A0000)

void initialize_vga();

void clear_memory_graphics();

void clear_back_buffer();

void clear_screen();

void clear_screen_color(uint8_t color);

void swap_buffer();

void graphics_set_pixel(uint16_t x, uint16_t y, uint8_t color);

void graphics_set_cursor(uint8_t x, uint8_t y);

void graphics_write_char(uint8_t x, uint8_t y, char c, uint8_t color);

void graphics_write_char_default(uint8_t x, uint8_t y, char c);

void graphics_write_string(uint8_t x, uint8_t y, const char *str, uint8_t color);

void graphics_write_string_default(uint8_t x, uint8_t y, const char *str);

void graphics_putchar(char c);

void graphics_puts(const char *str);

void graphics_backspace();

void graphics_scroll();

void graphics_set_block(uint8_t x, uint8_t y, uint8_t color);

void draw_cursor();

void erase_cursor();

#endif
