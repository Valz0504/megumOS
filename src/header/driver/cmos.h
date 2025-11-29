#ifndef _DRIVER_CMOS_H
#define _DRIVER_CMOS_H

#include <stdint.h>

struct RTCData {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
};

void read_rtc(struct RTCData *rtc);

#endif
