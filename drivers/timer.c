#include "timer.h"
#include "ports.h"
#include "keyboard.h"
#include "../cpu/isr.h"
#include "../kernel/task.h"

static volatile uint32_t tick_count = 0;
static uint32_t timer_freq = 0;
static int preempt_enabled = 0;

static void timer_callback(registers_t *regs) {
    (void)regs;
    tick_count++;

    // Preemptive scheduling: switch tasks every 10ms tick
    if (preempt_enabled && (tick_count % 5) == 0) {
        task_schedule();
    }
}

void timer_enable_preempt(void) {
    preempt_enabled = 1;
}

void timer_disable_preempt(void) {
    preempt_enabled = 0;
}

void timer_init(uint32_t frequency) {
    timer_freq = frequency;
    tick_count = 0;

    register_interrupt_handler(32, timer_callback); // IRQ0 = IDT 32

    // PIT runs at 1193180 Hz base frequency
    uint32_t divisor = 1193180 / frequency;

    // Command: channel 0, lobyte/hibyte, rate generator
    port_byte_out(0x43, 0x36);
    port_byte_out(0x40, (uint8_t)(divisor & 0xFF));
    port_byte_out(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

uint32_t timer_get_seconds(void) {
    if (timer_freq == 0) return 0;
    return tick_count / timer_freq;
}

void timer_wait(uint32_t ticks) {
    uint32_t target = tick_count + ticks;
    while (tick_count < target) {
        if (ctrl_c_pressed) return;
        __asm__ volatile("hlt");
    }
}
