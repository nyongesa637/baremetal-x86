#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);

// Key buffer for the shell
#define KEY_BUFFER_SIZE 256
extern char key_buffer[KEY_BUFFER_SIZE];
extern int key_buffer_len;

// Ctrl+C interrupt flag
extern volatile int ctrl_c_pressed;

#endif
