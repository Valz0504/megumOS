#include "../header/graphics/graphics.h"
#include "../header/cpu/portio.h"
#include "../header/graphics/font.h"
#include "../header/graphics/wallpaper.h"
#include "../header/stdlib/string.h"

static inline void out2(uint16_t port, uint16_t data) {
    out(port, (uint8_t)(data & 0xFF));
    out(port + 1, (uint8_t)((data >> 8) & 0xFF));
}

void initialize_vga()
{
    out(MISC_OUT_REG, 0x63);
    out2(CRT_ADDR_REG, (uint16_t)((0x0e << 8) + 0x11));

    out2(CRT_ADDR_REG, (uint16_t)((0x5f << 8) + 0x0));
    out2(CRT_ADDR_REG, (uint16_t)((0x4f << 8) + 0x1));
    out2(CRT_ADDR_REG, (uint16_t)((0x50 << 8) + 0x2));
    out2(CRT_ADDR_REG, (uint16_t)((0x82 << 8) + 0x3));
    out2(CRT_ADDR_REG, (uint16_t)((0x54 << 8) + 0x4));
    out2(CRT_ADDR_REG, (uint16_t)((0x80 << 8) + 0x5));
    out2(CRT_ADDR_REG, (uint16_t)((0xbf << 8) + 0x6));
    out2(CRT_ADDR_REG, (uint16_t)((0x1f << 8) + 0x7));
    out2(CRT_ADDR_REG, (uint16_t)((0x00 << 8) + 0x8));
    out2(CRT_ADDR_REG, (uint16_t)((0x41 << 8) + 0x9));
    out2(CRT_ADDR_REG, (uint16_t)((0x9c << 8) + 0x10));
    out2(CRT_ADDR_REG, (uint16_t)((0x8e << 8) + 0x11));
    out2(CRT_ADDR_REG, (uint16_t)((0x8f << 8) + 0x12));
    out2(CRT_ADDR_REG, (uint16_t)((0x28 << 8) + 0x13));
    out2(CRT_ADDR_REG, (uint16_t)((0x40 << 8) + 0x14));
    out2(CRT_ADDR_REG, (uint16_t)((0x96 << 8) + 0x15));
    out2(CRT_ADDR_REG, (uint16_t)((0xb9 << 8) + 0x16));
    out2(CRT_ADDR_REG, (uint16_t)((0xa3 << 8) + 0x17));

    out2(MEM_MODE_REG, (uint16_t)((0x01 << 8) + 0x01));
    out2(MEM_MODE_REG, (uint16_t)((0x0f << 8) + 0x02));
    out2(MEM_MODE_REG, (uint16_t)((0x00 << 8) + 0x03));
    out2(MEM_MODE_REG, (uint16_t)((0x0e << 8) + 0x04));

    out2(GRAP_ADDR_REG, (uint16_t)((0x40 << 8) + 0x05));
    out2(GRAP_ADDR_REG, (uint16_t)((0x05 << 8) + 0x06));

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x10);
    out2(ATTR_ADDR_REG, 0x41);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x11);
    out2(ATTR_ADDR_REG, 0x00);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x12);
    out2(ATTR_ADDR_REG, 0x0f);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x13);
    out2(ATTR_ADDR_REG, 0x00);

    in(INPUT_STATUS_1);
    out(ATTR_ADDR_REG, 0x14);
    out2(ATTR_ADDR_REG, 0x00);

    uint8_t paletteArray[768] = {
12,33,59,5,2,15,58,48,60,34,3,57,
21,12,37,56,22,60,0,0,0,42,23,50,
11,5,27,19,12,48,26,12,57,36,42,48,
63,57,62,24,10,32,42,16,62,18,10,30,
6,20,55,33,47,61,37,22,32,24,12,40,
2,1,6,62,22,62,55,32,44,24,3,47,
51,37,58,10,6,18,16,5,44,11,7,33,
2,10,44,50,46,58,46,24,62,14,8,26,
38,16,54,14,42,60,62,34,62,10,0,22,
17,2,35,32,13,63,63,49,56,4,16,38,
15,3,24,3,1,10,26,9,44,18,2,56,
55,12,62,38,10,60,33,17,50,45,12,62,
22,30,56,5,2,20,22,14,60,30,21,55,
48,38,52,12,6,30,15,14,35,29,21,48,
19,9,26,42,2,62,29,8,48,52,40,53,
14,9,40,8,5,30,33,19,61,4,3,25,
24,13,53,32,15,26,18,17,55,20,2,42,
10,2,36,56,44,56,14,1,32,2,13,54,
10,23,44,63,54,60,6,2,10,22,38,61,
28,7,38,28,2,61,6,2,23,62,44,62,
19,12,19,5,36,62,50,29,40,41,19,57,
34,11,42,3,25,61,25,12,48,60,4,62,
17,8,51,46,24,36,2,0,4,4,1,12,
35,26,51,30,4,46,12,1,24,28,22,28,
14,3,38,53,5,62,29,14,46,56,28,61,
46,18,62,28,34,59,62,28,62,19,6,30,
38,16,29,12,4,22,47,44,48,4,20,47,
17,11,44,2,21,57,48,26,55,36,18,44,
21,12,49,18,9,41,37,33,38,21,10,42,
14,25,34,52,12,59,50,29,60,12,3,18,
63,61,63,38,28,43,3,13,48,62,18,62,
2,6,37,36,2,62,18,2,39,23,8,38,
28,13,51,18,2,50,15,6,32,62,50,61,
38,8,54,36,24,61,58,40,60,46,36,48,
9,24,51,4,6,32,4,4,28,14,8,35,
50,22,62,50,11,62,28,22,62,43,18,32,
12,7,23,34,10,56,30,9,54,40,54,62,
21,12,52,27,29,45,30,40,62,2,0,1,
8,5,12,28,12,62,42,22,62,26,17,59,
4,16,43,25,4,33,14,6,28,8,50,62,
32,15,56,32,9,61,15,7,15,40,28,56,
21,20,60,9,4,24,12,10,44,31,35,50,
20,10,38,4,10,24,8,2,15,43,34,52,
42,32,46,11,7,20,58,37,48,25,8,52,
40,4,58,56,34,59,38,14,61,3,10,40,
20,11,34,3,1,8,27,20,39,19,18,24,
52,49,53,14,7,20,20,2,62,47,4,62,
50,20,56,25,12,44,8,3,20,10,8,38,
21,14,41,9,5,16,42,38,42,4,12,33,
9,1,19,4,15,60,24,2,37,4,30,62,
46,8,60,18,31,47,33,12,50,20,10,46,
27,8,59,16,26,53,8,27,57,25,16,45,
17,9,33,43,14,50,54,42,54,13,1,28,
2,4,16,59,57,60,13,2,48,16,8,23,
42,10,57,8,4,26,10,2,42,26,12,22,
26,16,51,57,17,62,5,2,7,51,16,62,
16,9,28,26,45,62,34,4,50,15,12,47,
18,2,28,3,21,62,0,0,1,0,0,3,
61,12,62,57,52,57,16,8,38,26,4,42,
49,14,54,7,3,12,60,53,61,27,2,51,
32,28,34,13,20,40,63,52,58,17,13,52,
2,19,53,42,10,62,44,41,55,30,3,54,
7,3,17,20,7,34,55,10,36,5,1,17,
58,46,56,22,7,58,22,14,56,21,3,32,

};

    out(DAC_WRITE_REG, 0);
    for (int i = 0; i < 768; i += 3)
    {
        out(DAC_DATA_REG, (uint8_t)paletteArray[i]);
        out(DAC_DATA_REG, (uint8_t)paletteArray[i + 1]);
        out(DAC_DATA_REG, (uint8_t)paletteArray[i + 2]);
    }

    // Override the default text color slot to a bright green
    out(DAC_WRITE_REG, DEFAULT_COLOR_FG);
    out(DAC_DATA_REG, 0);
    out(DAC_DATA_REG, 63);
    out(DAC_DATA_REG, 0);

    out(ATTR_ADDR_REG, 0x20);
    return;
}

uint8_t colLimit = SCREEN_WIDTH/BLOCK_WIDTH;
uint8_t rowLimit = SCREEN_HEIGHT/BLOCK_HEIGHT;
uint8_t rowDisplayLimit = SCREEN_HEIGHT/BLOCK_HEIGHT - 1;

char screenChar[SCREEN_HEIGHT/BLOCK_HEIGHT][SCREEN_WIDTH/BLOCK_WIDTH];
char screenColor[SCREEN_HEIGHT/BLOCK_HEIGHT][SCREEN_WIDTH/BLOCK_WIDTH];

uint8_t backBuffer[320*200];

void clear_memory_graphics() {
    memcpy(MEMORY_GRAPHICS, wallpaper, sizeof(wallpaper));
    return;
}

void clear_back_buffer() {
    memcpy(backBuffer, wallpaper, sizeof(wallpaper));
}

void clear_screen() {
    clear_memory_graphics();
    clear_back_buffer();
    for (uint8_t i = 0; i < rowLimit; ++i) {
        for (uint8_t j = 0; j < colLimit; ++j) {
            screenChar[i][j] = 0;
            screenColor[i][j] = 0;
        }
    }
    graphics_set_cursor(0, 0);
    return;
}

void swap_buffer() {
    memcpy(MEMORY_GRAPHICS, backBuffer, sizeof(backBuffer));
    return;
}

void graphics_set_pixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    backBuffer[y * SCREEN_WIDTH + x] = color;
    return;
}

static uint8_t graphics_cursor_row = 0;
static uint8_t graphics_cursor_col = 0;

void graphics_set_cursor(uint8_t x, uint8_t y) {
    if (x >= colLimit || y >= rowLimit) return;
    graphics_cursor_col = x;
    graphics_cursor_row = y;
    return;
}

void graphics_write_char(uint8_t x, uint8_t y, char c, uint8_t color){
    uint8_t index = (uint8_t) c;
    uint16_t blockStartX = x*BLOCK_WIDTH;
    uint16_t blockStartY = y*BLOCK_HEIGHT;

    screenChar[y][x] = c;
    screenColor[y][x] = color;

    for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++) {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++) {
            graphics_set_pixel(blockStartX + dx, blockStartY + dy, wallpaper[(blockStartY + dy) * SCREEN_WIDTH + (blockStartX + dx)]);
            // graphics_set_pixel(blockStartX + dx, blockStartY + dy, DEFAULT_COLOR_BG);
        }
    }

    for(int i = 1; i < lookup[index][0]; i++) {
        graphics_set_pixel(blockStartX + (lookup[index][i] >> 4), blockStartY + (lookup[index][i] & 0x0f), color);
    }

    return;
}

void graphics_write_char_default(uint8_t x, uint8_t y, char c) {
    if (x >= colLimit || y >= rowLimit) return;
    graphics_write_char(x, y, c, DEFAULT_COLOR_FG);
    return;
}

void graphics_write_string(uint8_t x, uint8_t y, const char *str, uint8_t color) {
    if (x >= colLimit || y >= rowLimit) return;

    uint8_t startX = x;
    uint8_t startY = y;

    while (*str) {
        if (*str == '\n') {
            startX = 0;
            startY++;
            if (startY >= rowLimit) break;
        } else {
            graphics_write_char(startX, startY, *str, color);
            startX++;
            if (startX >= colLimit) {
                startX = 0;
                startY++;
                if (startY >= rowLimit) break;
            }
        }
        str++;
    }
}

void graphics_write_string_default(uint8_t x, uint8_t y, const char *str) {
    if (x >= colLimit || y >= rowLimit) return;

    graphics_write_string(x, y, str, DEFAULT_COLOR_FG);
}

void graphics_putchar(char c) {
    erase_cursor();

    if (c == '\n') {
        graphics_cursor_col = 0;
        graphics_cursor_row++;
        if (graphics_cursor_row >= rowDisplayLimit) {
            graphics_scroll();
        }
    } else if (c == '\r') {
        graphics_cursor_col = 0;
    } else if (c == '\b') {
        graphics_backspace();
    } else if (c >= 32 && c <= 126) {
        graphics_write_char(graphics_cursor_col, graphics_cursor_row, c, DEFAULT_COLOR_FG);
        graphics_cursor_col++;
        if (graphics_cursor_col >= colLimit) {
            graphics_cursor_col = 0;
            graphics_cursor_row++;
            if(graphics_cursor_row >= rowDisplayLimit) {
                graphics_scroll();
            }
        }
    }

    draw_cursor();
    swap_buffer();
}

void graphics_puts(const char* str) {
    while (*str) {
        graphics_putchar(*str);
        str++;
    }
}

void graphics_backspace() {
    erase_cursor();

    if (graphics_cursor_col > 0) {
        graphics_cursor_col--;
    } else if (graphics_cursor_row > 0) {
        graphics_cursor_row--;
        graphics_cursor_col = colLimit - 1;
    }
    
    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;
    for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++) {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++) {
            graphics_set_pixel(startX + dx, startY + dy, wallpaper[(startY + dy) * SCREEN_WIDTH + (startX + dx)]);
            // graphics_set_pixel(startX + dx, startY + dy, DEFAULT_COLOR_BG);
        }
    }
    
    swap_buffer();
}

void graphics_scroll() {
    for (uint8_t y = 1; y < rowDisplayLimit; y++) {
        for (uint8_t x = 0; x < colLimit; x++) {
            screenChar[y - 1][x] = screenChar[y][x];
            screenColor[y - 1][x] = screenColor[y][x];
        }
    }

    for (uint8_t x = 0; x < colLimit; x++) {
        screenChar[rowDisplayLimit - 1][x] = 0;
        screenColor[rowDisplayLimit - 1][x] = DEFAULT_COLOR_BG;
    }

    for (uint8_t y = 0; y < rowDisplayLimit; y++) {
        for (uint8_t x = 0; x < colLimit; x++) {
            uint16_t startX = x * BLOCK_WIDTH;
            uint16_t startY = y * BLOCK_HEIGHT;
            for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++) {
                for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++) {
                    graphics_set_pixel(startX + dx, startY + dy, wallpaper[(startY + dy) * SCREEN_WIDTH + (startX + dx)]);
                    // graphics_set_pixel(startX + dx, startY + dy, DEFAULT_COLOR_BG);
                }
            }
        }
    }

    for (uint8_t y = 0; y < rowDisplayLimit; y++) {
        for (uint8_t x = 0; x < colLimit; x++) {
            if (screenChar[y][x] != 0) {
                graphics_write_char(x, y, screenChar[y][x], screenColor[y][x]);
            }
        }
    }

    graphics_cursor_row = rowDisplayLimit - 1;
    graphics_cursor_col = 0;

    swap_buffer();
}

void graphics_set_block(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= colLimit || y >= rowLimit) return;

    uint16_t startX = x * BLOCK_WIDTH;
    uint16_t startY = y * BLOCK_HEIGHT;

    for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++) {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++) {
            graphics_set_pixel(startX + dx, startY + dy, color);
        }
    }
}

void draw_cursor() {
    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;

    for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++) {
        graphics_set_pixel(startX, startY + dy, CURSOR_COLOR);
    }
}

void erase_cursor() {
    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;

    for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++) {
        graphics_set_pixel(startX, startY + dy, wallpaper[(startY + dy) * SCREEN_WIDTH + startX]);
        // graphics_set_pixel(startX, startY + dy, DEFAULT_COLOR_BG);
    }
}
