#include <stdbool.h>
#include "../header/graphics/graphics.h"
#include "../header/cpu/portio.h"
#include "../header/graphics/font.h"
#include "../header/graphics/wallpaper.h"
#include "../header/stdlib/string.h"
#include "palette_generated.h"

static bool wallpaper_background = true;
static uint8_t solid_bg_color = DEFAULT_COLOR_BG;

static inline uint8_t background_pixel(uint16_t x, uint16_t y)
{
    if (wallpaper_background)
    {
        return wallpaper[(uint32_t)y * SCREEN_WIDTH + x];
    }
    return solid_bg_color;
}

static inline void out2(uint16_t port, uint16_t data)
{
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

    // Use the generated palette from palette_generated.h
    out(DAC_WRITE_REG, 0);
    for (int i = 0; i < 768; i += 3)
    {
        out(DAC_DATA_REG, (uint8_t)mascot_palette[i]);
        out(DAC_DATA_REG, (uint8_t)mascot_palette[i + 1]);
        out(DAC_DATA_REG, (uint8_t)mascot_palette[i + 2]);
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

uint8_t colLimit = SCREEN_WIDTH / BLOCK_WIDTH;
uint8_t rowLimit = SCREEN_HEIGHT / BLOCK_HEIGHT;
uint8_t rowDisplayLimit = SCREEN_HEIGHT / BLOCK_HEIGHT - 1;

char screenChar[SCREEN_HEIGHT / BLOCK_HEIGHT][SCREEN_WIDTH / BLOCK_WIDTH];
char screenColor[SCREEN_HEIGHT / BLOCK_HEIGHT][SCREEN_WIDTH / BLOCK_WIDTH];

uint8_t backBuffer[320 * 200];

void clear_memory_graphics()
{
    if (wallpaper_background)
    {
        memcpy(MEMORY_GRAPHICS, wallpaper, sizeof(wallpaper));
    }
    else
    {
        uint32_t total = SCREEN_WIDTH * SCREEN_HEIGHT;
        for (uint32_t i = 0; i < total; ++i)
        {
            MEMORY_GRAPHICS[i] = solid_bg_color;
        }
    }
    return;
}

void clear_back_buffer()
{
    if (wallpaper_background)
    {
        memcpy(backBuffer, wallpaper, sizeof(wallpaper));
    }
    else
    {
        uint32_t total = SCREEN_WIDTH * SCREEN_HEIGHT;
        for (uint32_t i = 0; i < total; ++i)
        {
            backBuffer[i] = solid_bg_color;
        }
    }
}

void clear_screen()
{
    wallpaper_background = true;
    solid_bg_color = DEFAULT_COLOR_BG;
    clear_memory_graphics();
    clear_back_buffer();
    for (uint8_t i = 0; i < rowLimit; ++i)
    {
        for (uint8_t j = 0; j < colLimit; ++j)
        {
            screenChar[i][j] = 0;
            screenColor[i][j] = 0;
        }
    }
    graphics_set_cursor(0, 0);
    return;
}

void clear_screen_color(uint8_t color)
{
    wallpaper_background = false;
    solid_bg_color = color;
    uint32_t total = SCREEN_WIDTH * SCREEN_HEIGHT;
    for (uint32_t i = 0; i < total; ++i)
    {
        MEMORY_GRAPHICS[i] = color;
        backBuffer[i] = color;
    }
    for (uint8_t i = 0; i < rowLimit; ++i)
    {
        for (uint8_t j = 0; j < colLimit; ++j)
        {
            screenChar[i][j] = 0;
            screenColor[i][j] = color;
        }
    }
    graphics_set_cursor(0, 0);
}

void swap_buffer()
{
    memcpy(MEMORY_GRAPHICS, backBuffer, sizeof(backBuffer));
    return;
}

void graphics_set_pixel(uint16_t x, uint16_t y, uint8_t color)
{
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return;
    backBuffer[y * SCREEN_WIDTH + x] = color;
    return;
}

static uint8_t graphics_cursor_row = 0;
static uint8_t graphics_cursor_col = 0;

void graphics_set_cursor(uint8_t x, uint8_t y)
{
    if (x >= colLimit || y >= rowLimit)
        return;
    graphics_cursor_col = x;
    graphics_cursor_row = y;
    return;
}

void graphics_write_char(uint8_t x, uint8_t y, char c, uint8_t color)
{
    uint8_t index = (uint8_t)c;
    uint16_t blockStartX = x * BLOCK_WIDTH;
    uint16_t blockStartY = y * BLOCK_HEIGHT;

    screenChar[y][x] = c;
    screenColor[y][x] = color;

    for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++)
    {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++)
        {
            graphics_set_pixel(blockStartX + dx, blockStartY + dy, background_pixel(blockStartX + dx, blockStartY + dy));
            // graphics_set_pixel(blockStartX + dx, blockStartY + dy, DEFAULT_COLOR_BG);
        }
    }

    for (int i = 1; i < lookup[index][0]; i++)
    {
        graphics_set_pixel(blockStartX + (lookup[index][i] >> 4), blockStartY + (lookup[index][i] & 0x0f), color);
    }

    return;
}

void graphics_write_char_default(uint8_t x, uint8_t y, char c)
{
    if (x >= colLimit || y >= rowLimit)
        return;
    graphics_write_char(x, y, c, DEFAULT_COLOR_FG);
    return;
}

void graphics_write_string(uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    if (x >= colLimit || y >= rowLimit)
        return;

    uint8_t startX = x;
    uint8_t startY = y;

    while (*str)
    {
        if (*str == '\n')
        {
            startX = 0;
            startY++;
            if (startY >= rowLimit)
                break;
        }
        else
        {
            graphics_write_char(startX, startY, *str, color);
            startX++;
            if (startX >= colLimit)
            {
                startX = 0;
                startY++;
                if (startY >= rowLimit)
                    break;
            }
        }
        str++;
    }
}

void graphics_write_string_default(uint8_t x, uint8_t y, const char *str)
{
    if (x >= colLimit || y >= rowLimit)
        return;

    graphics_write_string(x, y, str, DEFAULT_COLOR_FG);
}

void graphics_putchar(char c)
{
    erase_cursor();

    if (c == '\n')
    {
        graphics_cursor_col = 0;
        graphics_cursor_row++;
        if (graphics_cursor_row >= rowDisplayLimit)
        {
            graphics_scroll();
        }
    }
    else if (c == '\r')
    {
        graphics_cursor_col = 0;
    }
    else if (c == '\b')
    {
        graphics_backspace();
    }
    else if (c >= 32 && c <= 126)
    {
        graphics_write_char(graphics_cursor_col, graphics_cursor_row, c, DEFAULT_COLOR_FG);
        graphics_cursor_col++;
        if (graphics_cursor_col >= colLimit)
        {
            graphics_cursor_col = 0;
            graphics_cursor_row++;
            if (graphics_cursor_row >= rowDisplayLimit)
            {
                graphics_scroll();
            }
        }
    }

    draw_cursor();
    swap_buffer();
}

void graphics_puts(const char *str)
{
    while (*str)
    {
        graphics_putchar(*str);
        str++;
    }
}

void graphics_backspace()
{
    erase_cursor();

    if (graphics_cursor_col > 0)
    {
        graphics_cursor_col--;
    }
    else if (graphics_cursor_row > 0)
    {
        graphics_cursor_row--;
        graphics_cursor_col = colLimit - 1;
    }

    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;
    for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++)
    {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++)
        {
            graphics_set_pixel(startX + dx, startY + dy, background_pixel(startX + dx, startY + dy));
            // graphics_set_pixel(startX + dx, startY + dy, DEFAULT_COLOR_BG);
        }
    }

    swap_buffer();
}

void graphics_scroll()
{
    for (uint8_t y = 1; y < rowDisplayLimit; y++)
    {
        for (uint8_t x = 0; x < colLimit; x++)
        {
            screenChar[y - 1][x] = screenChar[y][x];
            screenColor[y - 1][x] = screenColor[y][x];
        }
    }

    for (uint8_t x = 0; x < colLimit; x++)
    {
        screenChar[rowDisplayLimit - 1][x] = 0;
        screenColor[rowDisplayLimit - 1][x] = DEFAULT_COLOR_BG;
    }

    for (uint8_t y = 0; y < rowDisplayLimit; y++)
    {
        for (uint8_t x = 0; x < colLimit; x++)
        {
            uint16_t startX = x * BLOCK_WIDTH;
            uint16_t startY = y * BLOCK_HEIGHT;
            for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++)
            {
                for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++)
                {
                    graphics_set_pixel(startX + dx, startY + dy, background_pixel(startX + dx, startY + dy));
                    // graphics_set_pixel(startX + dx, startY + dy, DEFAULT_COLOR_BG);
                }
            }
        }
    }

    for (uint8_t y = 0; y < rowDisplayLimit; y++)
    {
        for (uint8_t x = 0; x < colLimit; x++)
        {
            if (screenChar[y][x] != 0)
            {
                graphics_write_char(x, y, screenChar[y][x], screenColor[y][x]);
            }
        }
    }

    graphics_cursor_row = rowDisplayLimit - 1;
    graphics_cursor_col = 0;

    swap_buffer();
}

void graphics_set_block(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= colLimit || y >= rowLimit)
        return;

    uint16_t startX = x * BLOCK_WIDTH;
    uint16_t startY = y * BLOCK_HEIGHT;

    for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++)
    {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++)
        {
            graphics_set_pixel(startX + dx, startY + dy, color);
        }
    }
}

// Direct write to VGA memory - bypasses backBuffer and swap_buffer
// Used for overlay elements like clock that shouldn't overwrite app graphics
void graphics_write_char_direct(uint8_t x, uint8_t y, char c, uint8_t color)
{
    if (x >= colLimit || y >= rowLimit)
        return;

    uint8_t index = (uint8_t)c;
    uint16_t blockStartX = x * BLOCK_WIDTH;
    uint16_t blockStartY = y * BLOCK_HEIGHT;

    // Clear the character cell first (black background for overlay)
    for (uint8_t dx = 0; dx < BLOCK_WIDTH; dx++)
    {
        for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++)
        {
            MEMORY_GRAPHICS[(blockStartY + dy) * SCREEN_WIDTH + blockStartX + dx] = DEFAULT_COLOR_BG;
        }
    }

    // Draw the character pixels directly to VGA memory
    for (int i = 1; i < lookup[index][0]; i++)
    {
        uint8_t px = lookup[index][i] >> 4;
        uint8_t py = lookup[index][i] & 0x0f;
        MEMORY_GRAPHICS[(blockStartY + py) * SCREEN_WIDTH + blockStartX + px] = color;
    }
}

void draw_cursor()
{
    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;

    for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++)
    {
        graphics_set_pixel(startX, startY + dy, CURSOR_COLOR);
    }
}

void erase_cursor()
{
    uint16_t startX = graphics_cursor_col * BLOCK_WIDTH;
    uint16_t startY = graphics_cursor_row * BLOCK_HEIGHT;

    for (uint8_t dy = 0; dy < BLOCK_HEIGHT; dy++)
    {
        graphics_set_pixel(startX, startY + dy, background_pixel(startX, startY + dy));
        // graphics_set_pixel(startX, startY + dy, DEFAULT_COLOR_BG);
    }
}
