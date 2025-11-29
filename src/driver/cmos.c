#include "../header/driver/cmos.h"
#include "../header/cpu/portio.h"
#include <stdbool.h>

static uint8_t cmos_read(uint8_t reg) {
    out(0x70, reg);
    return in(0x71);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}

void read_rtc(struct RTCData *rtc) {
    if (rtc == NULL) return;

    // Wait until RTC update is not in progress
    while (cmos_read(0x0A) & 0x80) {}

    uint8_t sec   = cmos_read(0x00);
    uint8_t min   = cmos_read(0x02);
    uint8_t hour  = cmos_read(0x04);
    uint8_t reg_b = cmos_read(0x0B);

    if (!(reg_b & 0x04)) { // BCD mode
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour & 0x7F);
    }

    if (!(reg_b & 0x02)) { // 12-hour format
        bool pm = hour & 0x80;
        hour &= 0x7F;
        if (pm && hour < 12) hour = (hour + 12) % 24;
    }

    rtc->second = sec;
    rtc->minute = min;
    rtc->hour   = hour;
}
