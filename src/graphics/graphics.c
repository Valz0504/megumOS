#include <stdbool.h>
#include "../header/graphics/graphics.h"
#include "../header/cpu/portio.h"
#include "../header/graphics/font.h"
#include "../header/graphics/wallpaper.h"
#include "../header/stdlib/string.h"

static bool wallpaper_background = true;
static uint8_t solid_bg_color = DEFAULT_COLOR_BG;

static inline uint8_t background_pixel(uint16_t x, uint16_t y) {
    if (wallpaper_background) {
        return wallpaper[(uint32_t)y * SCREEN_WIDTH + x];
    }
    return solid_bg_color;
}

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
63,63,63,63,63,63,63,63,63,63,63,62,
63,63,61,63,63,62,62,63,62,63,62,63,
63,62,62,63,62,62,63,62,61,62,62,62,
62,62,62,62,62,62,61,62,62,60,61,61,
59,54,51,53,56,57,52,51,50,55,49,45,
52,46,43,41,48,53,39,43,46,33,44,53,
27,43,52,42,38,39,34,38,41,28,39,47,
29,35,40,23,39,49,23,36,45,20,36,47,
16,36,47,32,32,34,25,31,36,20,33,42,
20,30,37,29,25,28,22,25,29,19,26,32,
19,22,26,15,32,43,15,30,40,15,28,37,
15,26,34,15,24,33,15,22,27,14,20,28,
15,20,22,10,30,43,8,28,40,11,26,37,
5,26,39,8,23,38,3,23,38,11,23,32,
6,23,33,7,20,38,6,21,32,2,20,37,
3,20,33,11,21,29,10,19,27,7,20,29,
4,19,30,17,17,20,13,17,20,11,17,25,
11,16,19,13,13,16,11,13,15,10,13,15,
10,11,12,9,16,27,9,14,18,9,12,17,
9,12,13,9,11,13,9,10,11,9,10,10,
9,9,10,7,16,37,7,17,28,7,17,24,
7,15,23,7,13,30,7,13,23,6,11,26,
6,10,22,7,13,19,7,12,16,6,11,17,
7,11,12,6,10,19,7,10,13,7,10,11,
7,9,9,4,16,39,3,17,33,3,18,30,
3,15,30,4,17,27,3,16,26,4,14,26,
2,14,26,4,12,34,3,12,35,5,12,29,
2,12,27,5,11,29,5,10,25,3,10,28,
4,15,22,3,14,22,4,13,21,2,13,21,
4,12,22,1,11,21,4,11,18,2,11,18,
5,10,22,4,9,21,3,10,20,1,10,20,
4,10,17,4,10,14,2,10,17,2,10,15,
8,9,9,8,8,9,8,8,9,7,8,9,
7,9,11,7,8,9,7,8,8,7,8,9,
7,7,9,5,9,19,5,8,15,5,7,15,
5,6,14,6,8,9,6,7,9,5,7,9,
5,6,9,7,7,7,7,7,7,7,7,7,
6,7,7,5,7,7,7,6,7,6,6,7,
6,6,7,5,6,7,6,6,6,6,6,6,
5,6,6,5,5,5,5,5,5,5,6,7,
5,5,6,5,5,5,5,5,5,4,8,22,
2,8,22,4,8,18,2,8,18,4,8,15,
4,7,15,2,8,15,1,8,15,2,6,20,
2,7,16,4,7,15,2,6,15,3,7,13,
3,6,13,1,6,13,4,8,11,4,7,11,
2,7,11,4,6,10,2,6,11,3,6,11,
1,6,12,3,6,9,3,5,10,3,5,8,
4,5,6,4,5,5,2,5,11,1,5,11,
1,5,9,5,5,6,4,4,5,4,4,6,
4,4,5,4,4,4,4,3,6,3,4,8,
3,4,5,3,3,6,1,4,11,1,3,11,
1,4,8,1,3,8,1,4,6,1,3,6,
1,2,5,0,2,6,4,4,4,4,4,4,
3,4,4,4,3,3,3,3,3,3,3,4,
3,3,3,2,3,3,3,2,2,2,3,3,
2,2,3,2,2,2,2,2,2,2,2,2,
1,2,3,2,1,3,1,1,4,1,1,2,
1,1,2,1,1,3,0,0,3,1,1,1,
1,1,1,1,1,1,1,0,0,0,0,0,
0,1,4,0,0,1,0,0,1,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,4,
0,0,1,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,63,0,63,63,63,
0,0,0,0,0,0,0,0,0,0,0,0,
};

    out(DAC_WRITE_REG, 0);
    for (int i = 0; i < 768; i += 3)
    {
        out(DAC_DATA_REG, (uint8_t)paletteArray[i]);
        out(DAC_DATA_REG, (uint8_t)paletteArray[i + 1]);
        out(DAC_DATA_REG, (uint8_t)paletteArray[i + 2]);
    }

    // Override the background color slot to black
    out(DAC_WRITE_REG, DEFAULT_COLOR_BG);
    out(DAC_DATA_REG, 0);
    out(DAC_DATA_REG, 0);
    out(DAC_DATA_REG, 0);

    // Override the default text color slot to a bright green
    out(DAC_WRITE_REG, DEFAULT_COLOR_FG);
    out(DAC_DATA_REG, 0);
    out(DAC_DATA_REG, 63);
    out(DAC_DATA_REG, 0);

    // Reserve a white slot for ASCII video (color index COLOR_ASCII_WHITE)
    out(DAC_WRITE_REG, COLOR_ASCII_WHITE);
    out(DAC_DATA_REG, 63);
    out(DAC_DATA_REG, 63);
    out(DAC_DATA_REG, 63);

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
    if (wallpaper_background) {
        memcpy(MEMORY_GRAPHICS, wallpaper, sizeof(wallpaper));
    } else {
        uint32_t total = SCREEN_WIDTH * SCREEN_HEIGHT;
        for (uint32_t i = 0; i < total; ++i) {
            MEMORY_GRAPHICS[i] = solid_bg_color;
        }
    }
    return;
}

void clear_back_buffer() {
    if (wallpaper_background) {
        memcpy(backBuffer, wallpaper, sizeof(wallpaper));
    } else {
        uint32_t total = SCREEN_WIDTH * SCREEN_HEIGHT;
        for (uint32_t i = 0; i < total; ++i) {
            backBuffer[i] = solid_bg_color;
        }
    }
}

void clear_screen() {
    wallpaper_background = true;
    solid_bg_color = DEFAULT_COLOR_BG;
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

void clear_screen_color(uint8_t color) {
    wallpaper_background = false;
    solid_bg_color = color;
    uint32_t total = SCREEN_WIDTH * SCREEN_HEIGHT;
    for (uint32_t i = 0; i < total; ++i) {
        MEMORY_GRAPHICS[i] = color;
        backBuffer[i] = color;
    }
    for (uint8_t i = 0; i < rowLimit; ++i) {
        for (uint8_t j = 0; j < colLimit; ++j) {
            screenChar[i][j] = 0;
            screenColor[i][j] = color;
        }
    }
    graphics_set_cursor(0, 0);
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
            graphics_set_pixel(blockStartX + dx, blockStartY + dy, background_pixel(blockStartX + dx, blockStartY + dy));
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
            graphics_set_pixel(startX + dx, startY + dy, background_pixel(startX + dx, startY + dy));
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
                    graphics_set_pixel(startX + dx, startY + dy, background_pixel(startX + dx, startY + dy));
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

// Direct write to VGA memory - bypasses backBuffer and swap_buffer
// Used for overlay elements like clock that shouldn't overwrite app graphics
void graphics_write_char_direct(uint8_t x, uint8_t y, char c, uint8_t color) {
    if (x >= colLimit || y >= rowLimit) return;
    
    uint8_t index = (uint8_t)c;
    uint16_t blockStartX = x * BLOCK_WIDTH;
    uint16_t blockStartY = y * BLOCK_HEIGHT;
    
    // Clear the character cell first (black background for overlay)
    for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++) {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++) {
            MEMORY_GRAPHICS[(blockStartY + dy) * SCREEN_WIDTH + blockStartX + dx] = DEFAULT_COLOR_BG;
        }
    }
    
    // Draw the character pixels directly to VGA memory
    for (int i = 1; i < lookup[index][0]; i++) {
        uint8_t px = lookup[index][i] >> 4;
        uint8_t py = lookup[index][i] & 0x0f;
        MEMORY_GRAPHICS[(blockStartY + py) * SCREEN_WIDTH + blockStartX + px] = color;
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
        graphics_set_pixel(startX, startY + dy, background_pixel(startX, startY + dy));
        // graphics_set_pixel(startX, startY + dy, DEFAULT_COLOR_BG);
    }
}
