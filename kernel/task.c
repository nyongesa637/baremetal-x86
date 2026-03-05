#include "task.h"
#include "klog.h"
#include "../libc/string.h"
#include "../drivers/timer.h"

extern void switch_context(task_regs_t *old, task_regs_t *new_regs);

static task_t tasks[MAX_TASKS];
static int current_task = 0;
static int task_count_val = 0;


void task_init(void) {
    memset(tasks, 0, sizeof(tasks));

    tasks[0].id = 0;
    strcpy(tasks[0].name, "kernel");
    tasks[0].state = TASK_RUNNING;
    task_count_val = 1;
    current_task = 0;
}

int task_create(const char *name, void (*entry)(void)) {
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    task_t *t = &tasks[slot];
    memset(t, 0, sizeof(task_t));
    t->id = slot;
    strncpy(t->name, name, 31);
    t->state = TASK_READY;

    // Set up stack with task_exit as return address
    uint32_t *sp = (uint32_t *)(t->stack + TASK_STACK_SIZE);
    sp = (uint32_t *)((uint32_t)sp & ~0xF); // 16-byte align
    sp--;
    *sp = (uint32_t)task_exit; // when entry() returns, it goes to task_exit

    t->regs.eip = (uint32_t)entry; // jump directly to entry
    t->regs.esp = (uint32_t)sp;
    t->regs.ebp = (uint32_t)sp;
    t->regs.eflags = 0x202; // IF=1

    task_count_val++;
    return slot;
}

void task_yield(void) {
    task_schedule();
}

void task_exit(void) {
    tasks[current_task].state = TASK_DEAD;
    task_count_val--;

    klog("[task] exited: ");
    klog(tasks[current_task].name);
    klog("\n");

    task_schedule();

    for (;;) { __asm__ volatile("hlt"); }
}

void task_sleep_ticks(uint32_t ticks) {
    tasks[current_task].state = TASK_SLEEPING;
    tasks[current_task].wake_tick = timer_get_ticks() + ticks;
    task_schedule();
}

static int find_next_task(void) {
    uint32_t now = timer_get_ticks();

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && now >= tasks[i].wake_tick) {
            tasks[i].state = TASK_READY;
        }
    }

    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (current_task + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) {
            return idx;
        }
    }

    if (tasks[current_task].state == TASK_RUNNING) {
        return current_task;
    }

    return 0;
}

void task_schedule(void) {
    int next = find_next_task();
    if (next == current_task && tasks[current_task].state == TASK_RUNNING) {
        return;
    }

    int old = current_task;

    if (tasks[old].state == TASK_RUNNING) {
        tasks[old].state = TASK_READY;
    }

    current_task = next;
    tasks[next].state = TASK_RUNNING;

    switch_context(&tasks[old].regs, &tasks[next].regs);
}

int task_count(void) {
    return task_count_val;
}

task_t *task_get_current(void) {
    return &tasks[current_task];
}

task_t *task_get_list(void) {
    return tasks;
}
