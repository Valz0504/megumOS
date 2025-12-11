#include <stdint.h>
#include <stdbool.h>
#include "../header/stdlib/string.h"

// Minimal syscall wrapper (same ABI as user-shell if needed)
static void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

// VGA graphics constants
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define CELL_SIZE 8
#define GRID_COLS (SCREEN_WIDTH / CELL_SIZE)
#define GRID_ROWS (SCREEN_HEIGHT / CELL_SIZE)
#define MAX_SNAKE_LEN (GRID_COLS * GRID_ROWS)
#define VGA_MEMORY ((volatile uint8_t *)0xC00A0000)
#define COLOR_BG 248   
#define COLOR_SNAKE 250 
#define COLOR_FOOD 12   

// Simple pseudo-random generator
static uint32_t rng_state = 0xC0DE2025;
static uint32_t rng(void)
{
    rng_state = rng_state * 1664525 + 1013904223;
    return rng_state;
}

// Point inside the grid
struct Point
{
    uint8_t x;
    uint8_t y;
};

static struct Point snake[MAX_SNAKE_LEN];
static uint32_t snake_length;
static struct Point food;
static int8_t dir_x = 1;
static int8_t dir_y = 0;
static uint32_t score;

static inline void draw_cell(uint8_t col, uint8_t row, uint8_t color);

static void redraw_game(void)
{
    for (uint32_t i = 0; i < snake_length; ++i)
    {
        draw_cell(snake[i].x, snake[i].y, COLOR_SNAKE);
    }
    draw_cell(food.x, food.y, COLOR_FOOD);
}

static inline void draw_cell(uint8_t col, uint8_t row, uint8_t color)
{
    if (col >= GRID_COLS || row >= GRID_ROWS)
        return;
    volatile uint8_t *frame = VGA_MEMORY;
    for (uint8_t dy = 0; dy < CELL_SIZE; ++dy)
    {
        uint32_t y = ((uint32_t)row * CELL_SIZE + dy) * SCREEN_WIDTH;
        for (uint8_t dx = 0; dx < CELL_SIZE; ++dx)
        {
            frame[y + (uint32_t)col * CELL_SIZE + dx] = color;
        }
    }
}

static inline void restore_cell(uint8_t col, uint8_t row)
{
    if (col >= GRID_COLS || row >= GRID_ROWS)
        return;
    draw_cell(col, row, COLOR_BG);
}

static void print_score_detail(void);

static void place_food(void)
{
    for (uint32_t attempt = 0; attempt < 1024; ++attempt)
    {
        uint32_t value = rng();
        uint8_t cx = value % GRID_COLS;
        uint8_t cy = (value / GRID_COLS) % GRID_ROWS;
        bool blocked = false;
        for (uint32_t i = 0; i < snake_length; ++i)
        {
            if (snake[i].x == cx && snake[i].y == cy)
            {
                blocked = true;
                break;
            }
        }
        if (!blocked)
        {
            food.x = cx;
            food.y = cy;
            return;
        }
    }

    // fallback sequential search
    for (uint8_t y = 0; y < GRID_ROWS; ++y)
    {
        for (uint8_t x = 0; x < GRID_COLS; ++x)
        {
            bool blocked = false;
            for (uint32_t i = 0; i < snake_length; ++i)
            {
                if (snake[i].x == x && snake[i].y == y)
                {
                    blocked = true;
                    break;
                }
            }
            if (!blocked)
            {
                food.x = x;
                food.y = y;
                return;
            }
        }
    }
}

static void init_snake(void)
{
    snake_length = 4;
    uint8_t start_x = GRID_COLS / 2;
    uint8_t start_y = GRID_ROWS / 2;
    for (uint32_t i = 0; i < snake_length; ++i)
    {
        snake[i].x = start_x - i;
        snake[i].y = start_y;
        draw_cell(snake[i].x, snake[i].y, COLOR_SNAKE);
    }
    dir_x = 1;
    dir_y = 0;
    score = 0;
    rng_state ^= start_x | (start_y << 8);
    place_food();
    draw_cell(food.x, food.y, COLOR_FOOD);
}

static bool move_snake(void)
{
    struct Point head = snake[0];
    int next_x = (int)head.x + dir_x;
    int next_y = (int)head.y + dir_y;
    if (next_x < 0 || next_x >= GRID_COLS ||
        next_y < 0 || next_y >= GRID_ROWS)
    {
        return false;
    }

    for (uint32_t i = 0; i < snake_length; ++i)
    {
        if (snake[i].x == (uint8_t)next_x && snake[i].y == (uint8_t)next_y)
        {
            return false;
        }
    }

    struct Point old_tail = snake[snake_length - 1];
    for (int32_t idx = (int32_t)snake_length - 1; idx > 0; --idx)
    {
        snake[idx] = snake[idx - 1];
    }

    snake[0].x = (uint8_t)next_x;
    snake[0].y = (uint8_t)next_y;
    draw_cell(snake[0].x, snake[0].y, COLOR_SNAKE);

    bool ate_food = (snake[0].x == food.x && snake[0].y == food.y);
    if (ate_food)
    {
        score += 1;
        if (snake_length < MAX_SNAKE_LEN)
        {
            snake[snake_length] = old_tail;
            snake_length += 1;
        }
        place_food();
        draw_cell(food.x, food.y, COLOR_FOOD);
        print_score_detail();
        redraw_game();
    }
    else
    {
        restore_cell(old_tail.x, old_tail.y);
    }

    return true;
}

static void print(const char *text)
{
    if (!text)
        return;
    syscall(6, (uint32_t)text, strlen(text), 0x0F);
}

static void print_number(uint32_t value)
{
    char digits[12];
    uint32_t length = 0;
    if (value == 0)
    {
        digits[length++] = '0';
    }
    else
    {
        while (value > 0 && length < sizeof(digits))
        {
            digits[length++] = (char)('0' + (value % 10));
            value /= 10;
        }
        for (uint32_t i = 0; i < length / 2; ++i)
        {
            char temp = digits[i];
            digits[i] = digits[length - 1 - i];
            digits[length - 1 - i] = temp;
        }
    }
    if (length > 0)
    {
        syscall(6, (uint32_t)digits, length, 0x0F);
    }
}

static void print_score_detail(void)
{
    print("\rScore: ");
    print_number(score);
    print("  Length: ");
    print_number(snake_length);
    print("   ");
}

static void update_direction(char key)
{
    int8_t next_dx = dir_x;
    int8_t next_dy = dir_y;

    switch (key)
    {
    case 'w':
    case 'W':
        next_dx = 0;
        next_dy = -1;
        break;
    case 's':
    case 'S':
        next_dx = 0;
        next_dy = 1;
        break;
    case 'a':
    case 'A':
        next_dx = -1;
        next_dy = 0;
        break;
    case 'd':
    case 'D':
        next_dx = 1;
        next_dy = 0;
        break;
    default:
        return;
    }

    if (next_dx == -dir_x && next_dy == -dir_y)
    {
        return;
    }

    dir_x = next_dx;
    dir_y = next_dy;
}

static void frame_delay(void)
{
    uint32_t speed = 80000000; 
    if (score > 10)
    {
        uint32_t delta = (score - 10) * 3000000;
        if (delta >= (speed - 20000000))
        {
            speed = 20000000;
        }
        else
        {
            speed -= delta;
        }
    }
    for (volatile uint32_t i = 0; i < speed; ++i)
    {
        __asm__ volatile("nop");
    }
}

int main(void)
{
    print("Starting Snake (WASD to move, q to quit)\n");
    syscall(7, 0, 0, 0);

    syscall(18, 1, 0, 0);
    syscall(14, COLOR_BG, 0, 0);

    init_snake();
    print_score_detail();
    redraw_game();

    bool running = true;
    bool alive = true;

    while (running && alive)
    {
        char key = '\0';
        syscall(4, (uint32_t)&key, 1, 0);
        if (key == 'q' || key == 'Q')
        {
            running = false;
            break;
        }
        if (key != '\0')
        {
            update_direction(key);
        }

        alive = move_snake();
        frame_delay();
    }

    syscall(18, 0, 0, 0);

    syscall(8, 0, 0, 0);
    if (alive)
    {
        print("Quitting snake... ");
    }
    else
    {
        print("Game over! ");
    }
    print("\n");
    print_score_detail();
    print("\n");

    return 0;
}
