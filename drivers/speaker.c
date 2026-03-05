#include "speaker.h"
#include "ports.h"
#include "timer.h"

void speaker_beep(uint32_t freq, uint32_t duration_ms) {
    if (freq == 0) return;

    // Set PIT channel 2 to desired frequency
    uint32_t divisor = 1193180 / freq;
    port_byte_out(0x43, 0xB6); // Channel 2, lobyte/hibyte, square wave
    port_byte_out(0x42, (uint8_t)(divisor & 0xFF));
    port_byte_out(0x42, (uint8_t)((divisor >> 8) & 0xFF));

    // Enable speaker (bits 0 and 1 of port 0x61)
    uint8_t tmp = port_byte_in(0x61);
    port_byte_out(0x61, tmp | 0x03);

    // Wait
    timer_wait(duration_ms / 10);

    // Disable speaker
    port_byte_out(0x61, tmp & ~0x03);
}
