#include "../header/process/scheduler.h"
#include "../header/cpu/portio.h"
#include "../header/cpu/interrupt.h"
#include "../header/driver/cmos.h"
#include "../header/text/framebuffer.h"
#include "../header/graphics/graphics.h"
#include <stdbool.h>

#define FB_COLS (SCREEN_WIDTH / BLOCK_WIDTH)
#define FB_ROWS (SCREEN_HEIGHT / BLOCK_HEIGHT)

volatile uint32_t scheduler_ticks = 0;
static volatile bool clock_enabled = false;
static uint32_t last_clock_tick = 0;

static void clear_clock_area(uint8_t row, uint8_t col_start)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        framebuffer_write(row, col_start + i, ' ', 0x0F, 0x00);
    }
}

// Draw simple HH:MM:SS clock at requested position.
static void draw_clock(uint8_t row, uint8_t col_start)
{
    struct RTCData rtc;
    read_rtc(&rtc);

    // Convert to GMT+7 (Jakarta, Indonesia)
    uint8_t hour_gmt7 = rtc.hour + 7;
    if (hour_gmt7 >= 24)
    {
        hour_gmt7 -= 24;
    }

    char buf[9];
    buf[0] = (hour_gmt7 / 10) + '0';
    buf[1] = (hour_gmt7 % 10) + '0';
    buf[2] = ':';
    buf[3] = (rtc.minute / 10) + '0';
    buf[4] = (rtc.minute % 10) + '0';
    buf[5] = ':';
    buf[6] = (rtc.second / 10) + '0';
    buf[7] = (rtc.second % 10) + '0';
    buf[8] = '\0';

    for (uint8_t i = 0; i < 8; i++)
    {
        framebuffer_write(row, col_start + i, buf[i], 0x0F, 0x00);
    }
}

static void activate_timer_interrupt(void)
{
    __asm__ volatile("cli");

    uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
    out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
    out(PIT_CHANNEL_0_DATA_PIO, pit_timer_counter_to_fire & 0xFF);
    out(PIT_CHANNEL_0_DATA_PIO, (pit_timer_counter_to_fire >> 8) & 0xFF);

    // Unmask PIT timer IRQ on PIC1
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));

    __asm__ volatile("sti");
}

void scheduler_init(void)
{
    activate_timer_interrupt();
}

void scheduler_tick(struct InterruptFrame *frame)
{
    (void)frame;
    scheduler_ticks++;
    if (clock_enabled && scheduler_ticks - last_clock_tick >= PIT_TIMER_FREQUENCY)
    {
        draw_clock(FB_ROWS - 1, FB_COLS - 8);
        last_clock_tick = scheduler_ticks;
    }
    pic_ack(IRQ_TIMER);
}

void scheduler_clock_start(void)
{
    clock_enabled = true;
    last_clock_tick = scheduler_ticks - PIT_TIMER_FREQUENCY;
    scheduler_tick(NULL);
}

void scheduler_clock_stop(void)
{
    clock_enabled = false;
    clear_clock_area(FB_ROWS - 1, FB_COLS - 8);
}

bool scheduler_clock_status(void)
{
    return clock_enabled;
}
