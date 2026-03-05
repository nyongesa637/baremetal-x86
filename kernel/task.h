#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS      16
#define TASK_STACK_SIZE 4096

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip, eflags;
} task_regs_t;

typedef struct {
    int          id;
    char         name[32];
    task_state_t state;
    task_regs_t  regs;
    uint8_t      stack[TASK_STACK_SIZE];
    uint32_t     wake_tick;  // for sleeping
} task_t;

void task_init(void);
int  task_create(const char *name, void (*entry)(void));
void task_yield(void);
void task_exit(void);
void task_sleep_ticks(uint32_t ticks);
void task_schedule(void);
int  task_count(void);
task_t *task_get_current(void);
task_t *task_get_list(void);

#endif
