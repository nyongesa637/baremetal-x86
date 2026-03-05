#include "keyboard.h"
#include "ports.h"
#include "vga.h"
#include "../cpu/isr.h"
#include "../shell/shell.h"
#include "../libc/string.h"

char key_buffer[KEY_BUFFER_SIZE];
int key_buffer_len = 0;
volatile int ctrl_c_pressed = 0;

// Command history
#define HISTORY_SIZE 16
static char history[HISTORY_SIZE][KEY_BUFFER_SIZE];
static int history_count = 0;
static int history_pos = -1; // -1 = current input, 0 = most recent, etc.
static char saved_input[KEY_BUFFER_SIZE]; // saves current input when browsing history
static int saved_len = 0;

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

static void history_push(const char *cmd) {
    if (strlen(cmd) == 0) return;
    // Don't duplicate the last entry
    if (history_count > 0 && strcmp(history[0], cmd) == 0) return;

    // Shift everything down
    int max = history_count < HISTORY_SIZE - 1 ? history_count : HISTORY_SIZE - 1;
    for (int i = max; i > 0; i--) {
        strcpy(history[i], history[i - 1]);
    }
    strncpy(history[0], cmd, KEY_BUFFER_SIZE - 1);
    history[0][KEY_BUFFER_SIZE - 1] = '\0';
    if (history_count < HISTORY_SIZE) history_count++;
}

// Erase current input from screen and replace with new text
static void replace_input(const char *new_text) {
    // Erase current displayed input
    while (key_buffer_len > 0) {
        vga_backspace();
        key_buffer_len--;
    }
    // Write new text
    key_buffer_len = strlen(new_text);
    if (key_buffer_len >= KEY_BUFFER_SIZE) key_buffer_len = KEY_BUFFER_SIZE - 1;
    strncpy(key_buffer, new_text, key_buffer_len);
    key_buffer[key_buffer_len] = '\0';
    vga_print(key_buffer);
}

static void keyboard_callback(registers_t *regs) {
    (void)regs;
    uint8_t scancode = port_byte_in(0x60);

    // Key release (bit 7 set)
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {
            shift_held = 0;
        } else if (released == 0x1D) {
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

    // Ctrl+C
    if (ctrl_held && scancode == 0x2E) {
        ctrl_c_pressed = 1;
        vga_print("^C\n");
        key_buffer_len = 0;
        key_buffer[0] = '\0';
        history_pos = -1;
        shell_prompt();
        return;
    }

    // Editor mode: route input to editor buffer
    if (editor_mode && scancode < sizeof(scancode_ascii)) {
        char c = shift_held ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];
        static int elen = 0;
        if (c == '\b') {
            if (elen > 0) { elen--; editor_line_buf[elen] = '\0'; vga_backspace(); }
        } else if (c == '\n') {
            vga_putchar('\n');
            editor_line_buf[elen] = '\0';
            editor_line_ready = 1;
            elen = 0;
        } else if (c != 0 && elen < 254) {
            editor_line_buf[elen++] = c;
            editor_line_buf[elen] = '\0';
            vga_putchar(c);
        }
        return;
    }

    // Ctrl+L = clear screen
    if (ctrl_held && scancode == 0x26) {
        vga_clear();
        shell_prompt();
        vga_print(key_buffer);
        return;
    }

    // Up arrow (scancode 0x48)
    if (scancode == 0x48) {
        if (history_count == 0) return;
        if (history_pos == -1) {
            // Save current input
            strncpy(saved_input, key_buffer, key_buffer_len);
            saved_input[key_buffer_len] = '\0';
            saved_len = key_buffer_len;
            history_pos = 0;
        } else if (history_pos < history_count - 1) {
            history_pos++;
        } else {
            return;
        }
        replace_input(history[history_pos]);
        return;
    }

    // Down arrow (scancode 0x50)
    if (scancode == 0x50) {
        if (history_pos < 0) return;
        history_pos--;
        if (history_pos < 0) {
            // Restore saved input
            replace_input(saved_input);
        } else {
            replace_input(history[history_pos]);
        }
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
            if (key_buffer_len > 0) {
                key_buffer_len--;
                key_buffer[key_buffer_len] = '\0';
                vga_backspace();
            }
        } else if (c == '\n') {
            vga_putchar('\n');
            key_buffer[key_buffer_len] = '\0';
            history_push(key_buffer);
            history_pos = -1;
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
    history_count = 0;
    history_pos = -1;
    register_interrupt_handler(33, keyboard_callback);
}
