#ifndef SHELL_H
#define SHELL_H

void shell_init(void);
void shell_prompt(void);
void shell_execute(const char *input);

// Editor mode: when set, keyboard input goes to editor buffer
extern volatile int editor_mode;
extern volatile int editor_line_ready;
extern char editor_line_buf[256];

#endif
