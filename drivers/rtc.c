#include "rtc.h"
#include "ports.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg) {
    port_byte_out(CMOS_ADDR, reg);
    return port_byte_in(CMOS_DATA);
}

static int cmos_updating(void) {
    port_byte_out(CMOS_ADDR, 0x0A);
    return port_byte_in(CMOS_DATA) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void rtc_read(rtc_time_t *t) {
    while (cmos_updating());

    t->second  = cmos_read(0x00);
    t->minute  = cmos_read(0x02);
    t->hour    = cmos_read(0x04);
    t->weekday = cmos_read(0x06);
    t->day     = cmos_read(0x07);
    t->month   = cmos_read(0x08);
    t->year    = cmos_read(0x09);

    uint8_t status_b = cmos_read(0x0B);

    // Convert BCD to binary if needed
    if (!(status_b & 0x04)) {
        t->second  = bcd_to_bin(t->second);
        t->minute  = bcd_to_bin(t->minute);
        t->hour    = bcd_to_bin(t->hour & 0x7F) | (t->hour & 0x80);
        t->day     = bcd_to_bin(t->day);
        t->month   = bcd_to_bin(t->month);
        t->year    = bcd_to_bin(t->year);
    }

    // Convert 12-hour to 24-hour if needed
    if (!(status_b & 0x02) && (t->hour & 0x80)) {
        t->hour = ((t->hour & 0x7F) + 12) % 24;
    }

    t->year += 2000;
}
