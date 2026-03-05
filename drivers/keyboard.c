#include "keyboard.h"
#include "ports.h"
#include "vga.h"
#include "../cpu/isr.h"
#include "../shell/shell.h"

char key_buffer[KEY_BUFFER_SIZE];
int key_buffer_len = 0;

// US keyboard scancode to ASCII lookup (lowercase)
static const char scancode_ascii[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0, ' '
};

// Shift held versions
static const char scancode_ascii_shift[] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*', 0, ' '
};

static int shift_held = 0;
static int ctrl_held = 0;
volatile int ctrl_c_pressed = 0;

static void keyboard_callback(registers_t *regs) {
    (void)regs;
    uint8_t scancode = port_byte_in(0x60);

    // Key release (bit 7 set)
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {
            shift_held = 0;
        } else if (released == 0x1D) { // Left Ctrl release
            ctrl_held = 0;
        }
        return;
    }

    // Ctrl press
    if (scancode == 0x1D) {
        ctrl_held = 1;
        return;
    }

    // Shift press
    if (scancode == 0x2A || scancode == 0x36) {
        shift_held = 1;
        return;
    }

    // Ctrl+C detection (scancode 0x2E = 'c')
    if (ctrl_held && scancode == 0x2E) {
        ctrl_c_pressed = 1;
        vga_print("^C\n");
        // Reset input buffer
        key_buffer_len = 0;
        key_buffer[0] = '\0';
        shell_prompt();
        return;
    }

    if (scancode < sizeof(scancode_ascii)) {
        char c;
        if (shift_held) {
            c = scancode_ascii_shift[scancode];
        } else {
            c = scancode_ascii[scancode];
        }

        if (c == '\b') {
            // Backspace
            if (key_buffer_len > 0) {
                key_buffer_len--;
                key_buffer[key_buffer_len] = '\0';
                vga_backspace();
            }
        } else if (c == '\n') {
            vga_putchar('\n');
            key_buffer[key_buffer_len] = '\0';
            shell_execute(key_buffer);
            key_buffer_len = 0;
            key_buffer[0] = '\0';
            shell_prompt();
        } else if (c != 0 && key_buffer_len < KEY_BUFFER_SIZE - 1) {
            key_buffer[key_buffer_len++] = c;
            key_buffer[key_buffer_len] = '\0';
            vga_putchar(c);
        }
    }
}

void keyboard_init(void) {
    key_buffer_len = 0;
    key_buffer[0] = '\0';
    register_interrupt_handler(33, keyboard_callback); // IRQ1 = IDT 33
}
