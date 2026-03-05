#include "vga.h"
#include "ports.h"

static uint16_t *video_memory = (uint16_t *)VGA_MEMORY;
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_color = 0;

static uint8_t make_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

static uint16_t make_vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    port_byte_out(0x3D4, 0x0F);
    port_byte_out(0x3D5, (uint8_t)(pos & 0xFF));
    port_byte_out(0x3D4, 0x0E);
    port_byte_out(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void) {
    if (cursor_y >= VGA_HEIGHT) {
        // Move everything up one row
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            video_memory[i] = video_memory[i + VGA_WIDTH];
        }
        // Clear the last row
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            video_memory[i] = make_vga_entry(' ', current_color);
        }
        cursor_y = VGA_HEIGHT - 1;
    }
}

void vga_init(void) {
    current_color = make_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        video_memory[i] = make_vga_entry(' ', current_color);
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = make_color(fg, bg);
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3;
    } else {
        video_memory[cursor_y * VGA_WIDTH + cursor_x] = make_vga_entry(c, current_color);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    scroll();
    update_cursor();
}

void vga_backspace(void) {
    if (cursor_x > 0) {
        cursor_x--;
    } else if (cursor_y > 0) {
        cursor_y--;
        cursor_x = VGA_WIDTH - 1;
    }
    video_memory[cursor_y * VGA_WIDTH + cursor_x] = make_vga_entry(' ', current_color);
    update_cursor();
}

void vga_print(const char *str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

int vga_get_cursor_x(void) {
    return cursor_x;
}

void vga_print_color(const char *str, uint8_t fg, uint8_t bg) {
    uint8_t old = current_color;
    current_color = make_color(fg, bg);
    vga_print(str);
    current_color = old;
}

void vga_print_hex(uint32_t value) {
    char hex_chars[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex_chars[value & 0xF];
        value >>= 4;
    }
    vga_print(buf);
}

void vga_print_dec(uint32_t value) {
    if (value == 0) {
        vga_putchar('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    // Print in reverse
    for (int j = i - 1; j >= 0; j--) {
        vga_putchar(buf[j]);
    }
}
