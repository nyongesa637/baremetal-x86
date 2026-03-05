#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/timer.h"
#include "../drivers/serial.h"
#include "../libc/string.h"
#include "../kernel/memory.h"
#include "../kernel/klog.h"
#include "../kernel/pmm.h"
#include "../kernel/vmm.h"
#include "../kernel/ramfs.h"
#include "../kernel/task.h"
#include "../kernel/elf.h"
#include "../kernel/program.h"
#include "../libc/syscall.h"
#include "../drivers/pci.h"
#include "../drivers/keyboard.h"
#include "../net/net.h"
#include "../drivers/rtc.h"
#include "../cpu/cpuid.h"
#include "../kernel/env.h"
#include "../libc/rand.h"
#include "../drivers/speaker.h"
#include "../drivers/ports.h"

volatile int editor_mode = 0;
volatile int editor_line_ready = 0;
char editor_line_buf[256];

static void cmd_help(void);
static void cmd_clear(void);
static void cmd_info(void);
static void cmd_mem(void);
static void cmd_uptime(void);
static void cmd_echo(const char *args);
static void cmd_color(const char *args);
static void cmd_sleep(const char *args);
static void cmd_ticks(void);
static void cmd_serial(const char *args);
static void cmd_pages(void);
static void cmd_pagemap(const char *args);
static void cmd_ls(void);
static void cmd_cat(const char *args);
static void cmd_touch(const char *args);
static void cmd_write(const char *args);
static void cmd_rm(const char *args);
static void cmd_stat(const char *args);
static void cmd_ps(void);
static void cmd_run(const char *args);
static void cmd_kill(const char *args);
static void cmd_exec(const char *args);
static void cmd_progs(void);
static void cmd_net(void);
static void cmd_ping(const char *args);
static void cmd_resolve(const char *args);
static void cmd_pci(void);
static void cmd_date(void);
static void cmd_time(void);
static void cmd_cpuinfo(void);
static void cmd_hexdump(const char *args);
static void cmd_beep(const char *args);
static void cmd_grep(const char *args);
static void cmd_calc(const char *args);
static void cmd_env(void);
static void cmd_set(const char *args);
static void cmd_unset(const char *args);
static void cmd_edit(const char *args);
static void cmd_wc(const char *args);
static void cmd_head(const char *args);
static void cmd_tail(const char *args);
static void cmd_append(const char *args);
static void cmd_cp(const char *args);
static void cmd_snake(void);
static void cmd_alias(const char *args);
static void cmd_unalias(const char *args);

// Aliases
#define MAX_ALIASES 16
#define ALIAS_NAME_SIZE 32
#define ALIAS_CMD_SIZE 128
static char alias_names[MAX_ALIASES][ALIAS_NAME_SIZE];
static char alias_cmds[MAX_ALIASES][ALIAS_CMD_SIZE];
static int alias_count = 0;

static const char *alias_lookup(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(alias_names[i], name) == 0) return alias_cmds[i];
    }
    return 0;
}

void shell_init(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("\n");
    vga_print("  _   _ _                         _  ___  ____  \n");
    vga_print(" | \\ | (_)_ __ ___  _ __ ___   __| |/ _ \\/ ___| \n");
    vga_print(" |  \\| | | '_ ` _ \\| '__/ _ \\ / _` | | | \\___ \\ \n");
    vga_print(" | |\\  | | | | | | | | | (_) | (_| | |_| |___) |\n");
    vga_print(" |_| \\_|_|_| |_| |_|_|  \\___/ \\__,_|\\___/|____/ \n");
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print(" NimrodOS v0.1.0 - A kernel built from scratch\n");
    vga_print(" Type 'help' for available commands.\n\n");
    shell_prompt();
}

void shell_prompt(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("nimrod");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("@");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("kernel");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print(" $ ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

void shell_execute(const char *input) {
    // Clear any pending Ctrl+C
    ctrl_c_pressed = 0;

    // Skip leading spaces
    while (*input == ' ') input++;

    if (strlen(input) == 0) return;

    // Alias expansion: check if first word matches an alias
    char expanded[256];
    {
        // Extract first word
        int wlen = 0;
        while (input[wlen] && input[wlen] != ' ') wlen++;
        char first[ALIAS_NAME_SIZE];
        if (wlen < ALIAS_NAME_SIZE) {
            strncpy(first, input, wlen);
            first[wlen] = '\0';
            const char *replacement = alias_lookup(first);
            if (replacement) {
                int rlen = strlen(replacement);
                int rest = strlen(input + wlen);
                if (rlen + rest < 255) {
                    strcpy(expanded, replacement);
                    strcpy(expanded + rlen, input + wlen);
                    input = expanded;
                }
            }
        }
    }

    if (strcmp(input, "help") == 0) {
        cmd_help();
    } else if (strcmp(input, "clear") == 0 || strcmp(input, "cls") == 0) {
        cmd_clear();
    } else if (strcmp(input, "info") == 0) {
        cmd_info();
    } else if (strcmp(input, "mem") == 0) {
        cmd_mem();
    } else if (starts_with(input, "echo ")) {
        cmd_echo(input + 5);
    } else if (starts_with(input, "color ")) {
        cmd_color(input + 6);
    } else if (strcmp(input, "halt") == 0 || strcmp(input, "shutdown") == 0) {
        vga_print("Halting system...\n");
        __asm__ volatile("cli; hlt");
    } else if (strcmp(input, "reboot") == 0) {
        vga_print("Rebooting...\n");
        // Triple fault to reboot
        uint8_t good = 0x02;
        while (good & 0x02) {
            good = 0;
            __asm__ volatile("inb $0x64, %0" : "=a"(good));
        }
        __asm__ volatile("outb %0, $0x64" : : "a"((uint8_t)0xFE));
    } else if (strcmp(input, "uptime") == 0) {
        cmd_uptime();
    } else if (starts_with(input, "sleep ")) {
        cmd_sleep(input + 6);
    } else if (strcmp(input, "ticks") == 0) {
        cmd_ticks();
    } else if (starts_with(input, "serial ")) {
        cmd_serial(input + 7);
    } else if (strcmp(input, "pages") == 0) {
        cmd_pages();
    } else if (starts_with(input, "pagemap ")) {
        cmd_pagemap(input + 8);
    } else if (strcmp(input, "ls") == 0) {
        cmd_ls();
    } else if (starts_with(input, "cat ")) {
        cmd_cat(input + 4);
    } else if (starts_with(input, "touch ")) {
        cmd_touch(input + 6);
    } else if (starts_with(input, "write ")) {
        cmd_write(input + 6);
    } else if (starts_with(input, "rm ")) {
        cmd_rm(input + 3);
    } else if (starts_with(input, "stat ")) {
        cmd_stat(input + 5);
    } else if (strcmp(input, "pid") == 0) {
        vga_print("  PID: ");
        vga_print_dec(sys_getpid());
        vga_print("\n");
    } else if (strcmp(input, "ps") == 0) {
        cmd_ps();
    } else if (starts_with(input, "run ")) {
        cmd_run(input + 4);
    } else if (starts_with(input, "kill ")) {
        cmd_kill(input + 5);
    } else if (starts_with(input, "exec ")) {
        cmd_exec(input + 5);
    } else if (strcmp(input, "progs") == 0) {
        cmd_progs();
    } else if (strcmp(input, "net") == 0 || strcmp(input, "netinfo") == 0) {
        cmd_net();
    } else if (starts_with(input, "ping ")) {
        cmd_ping(input + 5);
    } else if (starts_with(input, "resolve ")) {
        cmd_resolve(input + 8);
    } else if (strcmp(input, "pci") == 0) {
        cmd_pci();
    } else if (strcmp(input, "history") == 0) {
        int count = kbd_history_count();
        for (int i = count - 1; i >= 0; i--) {
            vga_set_color(VGA_DARK_GREY, VGA_BLACK);
            vga_print("  "); vga_print_dec(count - i); vga_print("  ");
            vga_set_color(VGA_WHITE, VGA_BLACK);
            vga_print(kbd_history_get(i)); vga_print("\n");
        }
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    } else if (strcmp(input, "rand") == 0 || starts_with(input, "rand ")) {
        rand_seed(timer_get_ticks());
        uint32_t max = 100;
        if (starts_with(input, "rand ")) {
            const char *a = input + 5;
            max = 0;
            while (*a >= '0' && *a <= '9') { max = max * 10 + (*a - '0'); a++; }
            if (max == 0) max = 100;
        }
        vga_print("  "); vga_print_dec(rand_range(1, max)); vga_print("\n");
    } else if (strcmp(input, "whoami") == 0) {
        vga_print("  "); vga_print(env_get("USER") ? env_get("USER") : "root"); vga_print("\n");
    } else if (strcmp(input, "hostname") == 0) {
        vga_print("  "); vga_print(env_get("HOSTNAME") ? env_get("HOSTNAME") : "baremetal"); vga_print("\n");
    } else if (starts_with(input, "edit ")) {
        cmd_edit(input + 5);
    } else if (starts_with(input, "wc ")) {
        cmd_wc(input + 3);
    } else if (starts_with(input, "head ")) {
        cmd_head(input + 5);
    } else if (starts_with(input, "tail ")) {
        cmd_tail(input + 5);
    } else if (starts_with(input, "append ")) {
        cmd_append(input + 7);
    } else if (starts_with(input, "cp ")) {
        cmd_cp(input + 3);
    } else if (strcmp(input, "env") == 0) {
        cmd_env();
    } else if (starts_with(input, "set ")) {
        cmd_set(input + 4);
    } else if (starts_with(input, "unset ")) {
        cmd_unset(input + 6);
    } else if (starts_with(input, "calc ")) {
        cmd_calc(input + 5);
    } else if (starts_with(input, "grep ")) {
        cmd_grep(input + 5);
    } else if (strcmp(input, "beep") == 0 || starts_with(input, "beep ")) {
        cmd_beep(strlen(input) > 4 ? input + 5 : "");
    } else if (starts_with(input, "hexdump ")) {
        cmd_hexdump(input + 8);
    } else if (strcmp(input, "cpuinfo") == 0) {
        cmd_cpuinfo();
    } else if (strcmp(input, "date") == 0) {
        cmd_date();
    } else if (strcmp(input, "time") == 0) {
        cmd_time();
    } else if (strcmp(input, "snake") == 0) {
        cmd_snake();
    } else if (strcmp(input, "alias") == 0 || starts_with(input, "alias ")) {
        cmd_alias(strlen(input) > 5 ? input + 6 : "");
    } else if (starts_with(input, "unalias ")) {
        cmd_unalias(input + 8);
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Unknown command: ");
        vga_print(input);
        vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    }
}

static void cmd_help(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== NimrodOS Commands ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  help      - Show this help message\n");
    vga_print("  clear     - Clear the screen\n");
    vga_print("  info      - System information\n");
    vga_print("  mem       - Memory usage\n");
    vga_print("  echo <s>  - Print text\n");
    vga_print("  color <n> - Set text color (0-15)\n");
    vga_print("  uptime    - System uptime\n");
    vga_print("  ticks     - Raw tick count\n");
    vga_print("  sleep <s> - Sleep for N seconds\n");
    vga_print("  serial <s>- Send text to serial port\n");
    vga_print("  pages     - Physical frame usage\n");
    vga_print("  pagemap <a>- Lookup virtual address\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Filesystem ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  ls        - List files\n");
    vga_print("  cat <f>   - Read a file\n");
    vga_print("  touch <f> - Create empty file\n");
    vga_print("  write <f> <text> - Write to file\n");
    vga_print("  rm <f>    - Delete a file\n");
    vga_print("  edit <f>  - Text editor\n");
    vga_print("  append <f> <text> - Append to file\n");
    vga_print("  cp <s> <d>- Copy file\n");
    vga_print("  wc <f>    - Word/line/byte count\n");
    vga_print("  head <f>  - First 10 lines\n");
    vga_print("  tail <f>  - Last 10 lines\n");
    vga_print("  stat <f>  - File info\n");
    vga_print("  hexdump <f>- Hex dump of file\n");
    vga_print("  grep <s> <f>- Search in file\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Tasks ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  ps        - List running tasks\n");
    vga_print("  run <name>- Run a built-in task\n");
    vga_print("  exec <f>  - Load & run ELF binary\n");
    vga_print("  progs     - List built-in programs\n");
    vga_print("  kill <id> - Kill a task\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Network ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  net       - Network status\n");
    vga_print("  ping <ip> - Ping an IP address\n");
    vga_print("  resolve <h>- DNS lookup\n");
    vga_print("  pci       - List PCI devices\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== System ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  cpuinfo   - CPU information\n");
    vga_print("  date      - Show current date\n");
    vga_print("  time      - Show current time\n");
    vga_print("  env       - Show all variables\n");
    vga_print("  set <k>=<v>- Set variable\n");
    vga_print("  unset <k> - Remove variable\n");
    vga_print("  calc <expr>- Calculator (+,-,*,/)\n");
    vga_print("  beep [hz] - PC speaker beep\n");
    vga_print("  history   - Command history\n");
    vga_print("  rand [max]- Random number\n");
    vga_print("  whoami    - Current user\n");
    vga_print("  hostname  - System hostname\n");
    vga_print("  alias [n=cmd] - Show/create alias\n");
    vga_print("  unalias <n>- Remove alias\n");
    vga_print("  halt      - Halt the CPU\n");
    vga_print("  reboot    - Reboot the system\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Games ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  snake     - Snake game (WASD/arrows)\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_clear(void) {
    vga_clear();
}

static void cmd_info(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== NimrodOS System Info ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  OS:       NimrodOS v0.1.0\n");
    vga_print("  Arch:     x86 (i686)\n");
    vga_print("  Kernel:   Custom monolithic kernel\n");
    vga_print("  Shell:    NimrodSH built-in\n");
    vga_print("  VGA:      80x25 text mode\n");
    vga_print("  Heap:     4 MB at 0x100000\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_mem(void) {
    uint32_t used = memory_used();
    uint32_t free_mem = memory_free();
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Memory Usage ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Used:  ");
    vga_print_dec(used / 1024);
    vga_print(" KB\n");
    vga_print("  Free:  ");
    vga_print_dec(free_mem / 1024);
    vga_print(" KB\n");
    vga_print("  Total: ");
    vga_print_dec((used + free_mem) / 1024);
    vga_print(" KB\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_echo(const char *args) {
    vga_print(args);
    vga_print("\n");
}

static void cmd_color(const char *args) {
    int color = 0;
    while (*args >= '0' && *args <= '9') {
        color = color * 10 + (*args - '0');
        args++;
    }
    if (color >= 0 && color <= 15) {
        vga_set_color((uint8_t)color, VGA_BLACK);
        vga_print("Color changed.\n");
    } else {
        vga_print("Invalid color. Use 0-15.\n");
    }
}

static void cmd_uptime(void) {
    uint32_t total_secs = timer_get_seconds();
    uint32_t hours = total_secs / 3600;
    uint32_t mins  = (total_secs % 3600) / 60;
    uint32_t secs  = total_secs % 60;

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Uptime: ");
    vga_print_dec(hours);
    vga_print("h ");
    vga_print_dec(mins);
    vga_print("m ");
    vga_print_dec(secs);
    vga_print("s\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_ticks(void) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Ticks: ");
    vga_print_dec(timer_get_ticks());
    vga_print(" (100 Hz)\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_sleep(const char *args) {
    int seconds = 0;
    while (*args >= '0' && *args <= '9') {
        seconds = seconds * 10 + (*args - '0');
        args++;
    }
    if (seconds <= 0 || seconds > 60) {
        vga_print("Usage: sleep <1-60>\n");
        return;
    }
    vga_print("Sleeping for ");
    vga_print_dec(seconds);
    vga_print("s...\n");
    timer_wait(seconds * 100);
    if (ctrl_c_pressed) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_print("Interrupted.\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_print("Done.\n");
    }
}

static void cmd_serial(const char *args) {
    serial_print(SERIAL_COM1, "[shell] ");
    serial_print(SERIAL_COM1, args);
    serial_print(SERIAL_COM1, "\n");
    vga_print("Sent to COM1: ");
    vga_print(args);
    vga_print("\n");
}

static void cmd_pages(void) {
    uint32_t used = pmm_frames_used();
    uint32_t free_f = pmm_frames_free();
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Physical Memory Frames ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Used:  ");
    vga_print_dec(used);
    vga_print(" frames (");
    vga_print_dec(used * 4);
    vga_print(" KB)\n");
    vga_print("  Free:  ");
    vga_print_dec(free_f);
    vga_print(" frames (");
    vga_print_dec(free_f * 4);
    vga_print(" KB)\n");
    vga_print("  Total: ");
    vga_print_dec(used + free_f);
    vga_print(" frames (");
    vga_print_dec((used + free_f) * 4);
    vga_print(" KB)\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static uint32_t parse_hex(const char *s) {
    uint32_t val = 0;
    if (s[0] == '0' && s[1] == 'x') s += 2;
    while (*s) {
        val <<= 4;
        if (*s >= '0' && *s <= '9') val |= *s - '0';
        else if (*s >= 'a' && *s <= 'f') val |= *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') val |= *s - 'A' + 10;
        else break;
        s++;
    }
    return val;
}

static void cmd_pagemap(const char *args) {
    uint32_t vaddr = parse_hex(args);
    uint32_t paddr = vmm_get_physical(vaddr);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Virtual:  ");
    vga_print_hex(vaddr);
    vga_print("\n  Physical: ");
    if (paddr) {
        vga_print_hex(paddr);
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("NOT MAPPED");
    }
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_ls(void) {
    char names[RAMFS_MAX_FILES][RAMFS_MAX_NAME];
    int count = ramfs_list(names, RAMFS_MAX_FILES);
    if (count == 0) {
        vga_print("  (no files)\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        vga_print("  ");
        vga_print(names[i]);
        vga_set_color(VGA_DARK_GREY, VGA_BLACK);
        vga_print("  ");
        vga_print_dec(ramfs_size(names[i]));
        vga_print(" bytes\n");
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print_dec(count);
    vga_print(" file(s)\n");
}

static void cmd_cat(const char *args) {
    while (*args == ' ') args++;
    if (!ramfs_exists(args)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: ");
        vga_print(args);
        vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }
    char buf[RAMFS_MAX_FILESIZE + 1];
    int n = ramfs_read(args, buf, RAMFS_MAX_FILESIZE);
    if (n > 0) {
        buf[n] = '\0';
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print(buf);
        // Ensure newline at end
        if (buf[n - 1] != '\n') vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    }
}

static void cmd_touch(const char *args) {
    while (*args == ' ') args++;
    if (ramfs_exists(args)) {
        vga_print("File already exists.\n");
        return;
    }
    int r = ramfs_create(args);
    if (r == -2) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Filesystem full.\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_print("Created: ");
        vga_print(args);
        vga_print("\n");
    }
}

static void cmd_write(const char *args) {
    // Parse: "filename content here"
    while (*args == ' ') args++;
    char filename[RAMFS_MAX_NAME];
    int i = 0;
    while (*args && *args != ' ' && i < RAMFS_MAX_NAME - 1) {
        filename[i++] = *args++;
    }
    filename[i] = '\0';

    if (*args == ' ') args++; // skip space between filename and content

    if (i == 0 || *args == '\0') {
        vga_print("Usage: write <filename> <content>\n");
        return;
    }

    int len = strlen(args);
    int n = ramfs_write(filename, args, len);
    if (n < 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Write failed.\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_print("Wrote ");
        vga_print_dec(n);
        vga_print(" bytes to ");
        vga_print(filename);
        vga_print("\n");
    }
}

static void cmd_rm(const char *args) {
    while (*args == ' ') args++;
    if (ramfs_delete(args) == 0) {
        vga_print("Deleted: ");
        vga_print(args);
        vga_print("\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: ");
        vga_print(args);
        vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    }
}

static void cmd_stat(const char *args) {
    while (*args == ' ') args++;
    if (!ramfs_exists(args)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: ");
        vga_print(args);
        vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Name: ");
    vga_print(args);
    vga_print("\n  Size: ");
    vga_print_dec(ramfs_size(args));
    vga_print(" bytes\n  Max:  ");
    vga_print_dec(RAMFS_MAX_FILESIZE);
    vga_print(" bytes\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

// --- Built-in background tasks (using syscalls) ---

static void task_counter(void) {
    for (int i = 1; i <= 10; i++) {
        serial_log("[counter] tick\n");
        sys_sleep(1);
    }
    serial_log("[counter] done\n");
}

static void task_logger(void) {
    for (int i = 0; i < 5; i++) {
        serial_log("[logger] heartbeat\n");
        sys_sleep(2);
    }
    serial_log("[logger] done\n");
}

static void task_memwatch(void) {
    for (int i = 0; i < 5; i++) {
        serial_log("[memwatch] checking heap\n");
        sys_sleep(3);
    }
    serial_log("[memwatch] done\n");
}

static void task_sysinfo(void) {
    // Writes system info to a file via syscalls
    sys_create("sysinfo.txt");
    int fd = sys_open("sysinfo.txt");
    if (fd >= 0) {
        sys_write(fd, "NimrodOS Runtime Info\n", 21);
        sys_close(fd);
    }
    print("sysinfo: wrote sysinfo.txt\n");
    serial_log("[sysinfo] wrote sysinfo.txt\n");
}

static const char *state_name(int state) {
    switch (state) {
        case TASK_UNUSED:   return "unused";
        case TASK_READY:    return "ready";
        case TASK_RUNNING:  return "running";
        case TASK_SLEEPING: return "sleeping";
        case TASK_DEAD:     return "dead";
        default:            return "???";
    }
}

static void cmd_ps(void) {
    task_t *list = task_get_list();
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Tasks ===\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("  ID  NAME            STATE\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    for (int i = 0; i < MAX_TASKS; i++) {
        if (list[i].state == TASK_UNUSED || list[i].state == TASK_DEAD) continue;
        vga_print("  ");
        vga_print_dec(list[i].id);
        // Pad to column
        if (list[i].id < 10) vga_print("   ");
        else vga_print("  ");
        vga_print(list[i].name);
        // Pad name
        int nlen = strlen(list[i].name);
        for (int p = nlen; p < 16; p++) vga_putchar(' ');
        if (list[i].state == TASK_RUNNING) {
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        } else if (list[i].state == TASK_SLEEPING) {
            vga_set_color(VGA_YELLOW, VGA_BLACK);
        } else {
            vga_set_color(VGA_WHITE, VGA_BLACK);
        }
        vga_print(state_name(list[i].state));
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print("\n");
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_run(const char *args) {
    while (*args == ' ') args++;

    void (*entry)(void) = 0;

    if (strcmp(args, "counter") == 0) {
        entry = task_counter;
    } else if (strcmp(args, "logger") == 0) {
        entry = task_logger;
    } else if (strcmp(args, "memwatch") == 0) {
        entry = task_memwatch;
    } else if (strcmp(args, "sysinfo") == 0) {
        entry = task_sysinfo;
    } else {
        vga_print("Available: counter, logger, memwatch, sysinfo\n");
        return;
    }

    int id = task_create(args, entry);
    if (id < 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Failed to create task (slots full).\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_print("Started task '");
        vga_print(args);
        vga_print("' (PID ");
        vga_print_dec(id);
        vga_print(")\n");
    }
}

static void cmd_kill(const char *args) {
    while (*args == ' ') args++;
    int id = 0;
    while (*args >= '0' && *args <= '9') {
        id = id * 10 + (*args - '0');
        args++;
    }
    if (id == 0) {
        vga_print("Cannot kill kernel task.\n");
        return;
    }
    task_t *list = task_get_list();
    if (id >= MAX_TASKS || list[id].state == TASK_UNUSED || list[id].state == TASK_DEAD) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("No such task.\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }
    list[id].state = TASK_DEAD;
    vga_print("Killed task ");
    vga_print_dec(id);
    vga_print(" (");
    vga_print(list[id].name);
    vga_print(")\n");
}

static void cmd_exec(const char *args) {
    while (*args == ' ') args++;
    if (strlen(args) == 0) {
        vga_print("Usage: exec <filename.elf>\n");
        return;
    }

    vga_print("Loading ");
    vga_print(args);
    vga_print("...\n");

    int tid = elf_load_and_run(args);
    if (tid < 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Failed to load ELF (error ");
        vga_print_dec(-tid);
        vga_print(")\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("Running as PID ");
        vga_print_dec(tid);
        vga_print("\n");
    }
}

static void cmd_progs(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Built-in Programs ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    for (int i = 0; i < builtin_program_count; i++) {
        vga_print("  ");
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        vga_print(builtin_programs[i].name);
        vga_set_color(VGA_DARK_GREY, VGA_BLACK);
        vga_print("  ");
        vga_print_dec(builtin_programs[i].size);
        vga_print("B  ");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print(builtin_programs[i].description);
        vga_print("\n");
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("Use 'exec <name>' to run.\n");
}

static uint32_t parse_ip(const char *s) {
    uint32_t octets[4] = {0};
    int idx = 0;
    while (*s && idx < 4) {
        if (*s >= '0' && *s <= '9') {
            octets[idx] = octets[idx] * 10 + (*s - '0');
        } else if (*s == '.') {
            idx++;
        }
        s++;
    }
    return (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
}

static void cmd_net(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Network Status ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    if (!net_is_up()) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("  Status: DOWN (no NIC found)\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("  Status: UP\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    char buf[24];
    net_format_mac(net_get_mac(), buf);
    vga_print("  MAC:    ");
    vga_print(buf);
    vga_print("\n");

    net_format_ip(htonl(net_get_ip()), buf);
    vga_print("  IP:     ");
    vga_print(buf);
    vga_print("\n");

    net_format_ip(htonl(NET_GATEWAY), buf);
    vga_print("  Gateway:");
    vga_print(buf);
    vga_print("\n");

    net_format_ip(htonl(NET_DNS), buf);
    vga_print("  DNS:    ");
    vga_print(buf);
    vga_print("\n");

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_ping(const char *args) {
    while (*args == ' ') args++;
    if (!net_is_up()) {
        vga_print("Network not available.\n");
        return;
    }

    uint32_t ip = parse_ip(args);
    if (ip == 0) {
        vga_print("Usage: ping <ip address>\n");
        return;
    }

    char ip_str[16];
    net_format_ip(htonl(ip), ip_str);
    vga_print("Pinging ");
    vga_print(ip_str);
    vga_print("...\n");

    for (int i = 0; i < 4; i++) {
        if (ctrl_c_pressed) break;
        net_send_icmp_echo(ip, 0x1234, i + 1);

        // Wait for reply (up to 2 seconds)
        uint32_t start = timer_get_ticks();
        ping_state_t *ps = net_get_ping_state();
        while (!ps->got_reply && (timer_get_ticks() - start) < 200) {
            if (ctrl_c_pressed) break;
            __asm__ volatile("hlt");
        }

        if (ctrl_c_pressed) break;

        if (ps->got_reply) {
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            vga_print("  Reply from ");
            vga_print(ip_str);
            vga_print(": time=");
            vga_print_dec(ps->rtt_ticks * 10);
            vga_print("ms\n");
        } else {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_print("  Request timed out.\n");
        }
        vga_set_color(VGA_WHITE, VGA_BLACK);

        // Wait 1 second between pings
        if (i < 3) timer_wait(100);
    }
    if (ctrl_c_pressed) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_print("  Interrupted.\n");
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_resolve(const char *args) {
    while (*args == ' ') args++;
    if (!net_is_up()) {
        vga_print("Network not available.\n");
        return;
    }
    if (strlen(args) == 0) {
        vga_print("Usage: resolve <hostname>\n");
        return;
    }

    vga_print("Resolving ");
    vga_print(args);
    vga_print("...\n");

    uint32_t ip = 0;
    if (net_dns_resolve(args, &ip) == 0) {
        char ip_str[16];
        net_format_ip(htonl(ip), ip_str);
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("  ");
        vga_print(args);
        vga_print(" -> ");
        vga_print(ip_str);
        vga_print("\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("  DNS lookup failed.\n");
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_pci(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== PCI Devices ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    int count = pci_device_count();
    pci_device_t *devs = pci_get_devices();

    for (int i = 0; i < count; i++) {
        vga_print("  ");
        vga_print_dec(devs[i].bus);
        vga_print(":");
        vga_print_dec(devs[i].slot);
        vga_print(".");
        vga_print_dec(devs[i].func);
        vga_print(" [");
        vga_print_hex(devs[i].vendor_id);
        vga_print(":");
        vga_print_hex(devs[i].device_id);
        vga_print("] class=");
        vga_print_hex(devs[i].class_code);
        vga_print(":");
        vga_print_hex(devs[i].subclass);
        vga_print(" IRQ=");
        vga_print_dec(devs[i].irq_line);
        vga_print("\n");
    }

    vga_print_dec(count);
    vga_print(" device(s)\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void print2d(uint32_t val) {
    if (val < 10) vga_putchar('0');
    vga_print_dec(val);
}

static const char *weekday_names[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char *month_names[] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static void cmd_date(void) {
    rtc_time_t t;
    rtc_read(&t);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  ");
    if (t.weekday < 7) { vga_print(weekday_names[t.weekday]); vga_print(", "); }
    if (t.month >= 1 && t.month <= 12) { vga_print(month_names[t.month]); vga_print(" "); }
    vga_print_dec(t.day);
    vga_print(", ");
    vga_print_dec(t.year);
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_time(void) {
    rtc_time_t t;
    rtc_read(&t);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  ");
    print2d(t.hour); vga_putchar(':');
    print2d(t.minute); vga_putchar(':');
    print2d(t.second);
    vga_print(" UTC\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_cpuinfo(void) {
    cpu_info_t info;
    cpuid_detect(&info);
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== CPU Information ===\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  Vendor: "); vga_print(info.vendor); vga_print("\n");
    vga_print("  Brand:  "); vga_print(info.brand); vga_print("\n");
    vga_print("  Family: "); vga_print_dec(info.family);
    vga_print("  Model: "); vga_print_dec(info.model);
    vga_print("  Stepping: "); vga_print_dec(info.stepping); vga_print("\n");
    vga_print("  Features:");
    if (info.has_fpu)  vga_print(" FPU");
    if (info.has_sse)  vga_print(" SSE");
    if (info.has_sse2) vga_print(" SSE2");
    if (info.has_sse3) vga_print(" SSE3");
    if (info.has_avx)  vga_print(" AVX");
    if (info.has_apic) vga_print(" APIC");
    if (info.has_msr)  vga_print(" MSR");
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_hexdump(const char *args) {
    while (*args == ' ') args++;
    if (!ramfs_exists(args)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: "); vga_print(args); vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }
    char buf[RAMFS_MAX_FILESIZE];
    int n = ramfs_read(args, buf, RAMFS_MAX_FILESIZE);
    if (n <= 0) { vga_print("  (empty)\n"); return; }

    static const char hex[] = "0123456789ABCDEF";
    for (int off = 0; off < n; off += 16) {
        if (ctrl_c_pressed) break;
        // Offset
        vga_set_color(VGA_DARK_GREY, VGA_BLACK);
        for (int s = 12; s >= 0; s -= 4)
            vga_putchar(hex[(off >> s) & 0xF]);
        vga_print("  ");
        // Hex bytes
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        for (int i = 0; i < 16; i++) {
            if (off + i < n) {
                uint8_t b = (uint8_t)buf[off + i];
                vga_putchar(hex[b >> 4]);
                vga_putchar(hex[b & 0xF]);
            } else {
                vga_print("  ");
            }
            vga_putchar(' ');
            if (i == 7) vga_putchar(' ');
        }
        // ASCII
        vga_print(" ");
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        for (int i = 0; i < 16 && off + i < n; i++) {
            char c = buf[off + i];
            vga_putchar((c >= 32 && c < 127) ? c : '.');
        }
        vga_putchar('\n');
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_beep(const char *args) {
    while (*args == ' ') args++;
    uint32_t freq = 800;
    if (*args >= '0' && *args <= '9') {
        freq = 0;
        while (*args >= '0' && *args <= '9') {
            freq = freq * 10 + (*args - '0');
            args++;
        }
    }
    if (freq < 20) freq = 20;
    if (freq > 20000) freq = 20000;
    vga_print("Beep at ");
    vga_print_dec(freq);
    vga_print(" Hz\n");
    speaker_beep(freq, 200);
}

static int strstr_pos(const char *haystack, const char *needle) {
    int nlen = strlen(needle);
    if (nlen == 0) return -1;
    for (int i = 0; haystack[i]; i++) {
        int match = 1;
        for (int j = 0; j < nlen; j++) {
            if (haystack[i + j] == '\0' || haystack[i + j] != needle[j]) {
                match = 0;
                break;
            }
        }
        if (match) return i;
    }
    return -1;
}

static void cmd_grep(const char *args) {
    while (*args == ' ') args++;
    char pattern[64];
    int pi = 0;
    while (*args && *args != ' ' && pi < 63) pattern[pi++] = *args++;
    pattern[pi] = '\0';
    while (*args == ' ') args++;

    if (pi == 0 || strlen(args) == 0) {
        vga_print("Usage: grep <pattern> <filename>\n");
        return;
    }

    if (!ramfs_exists(args)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: "); vga_print(args); vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }

    char buf[RAMFS_MAX_FILESIZE + 1];
    int n = ramfs_read(args, buf, RAMFS_MAX_FILESIZE);
    if (n <= 0) return;
    buf[n] = '\0';

    int line_num = 1;
    int matches = 0;
    char *line = buf;
    while (*line) {
        if (ctrl_c_pressed) break;
        char *eol = line;
        while (*eol && *eol != '\n') eol++;

        char saved = *eol;
        *eol = '\0';

        if (strstr_pos(line, pattern) >= 0) {
            vga_set_color(VGA_DARK_GREY, VGA_BLACK);
            vga_print_dec(line_num);
            vga_print(": ");
            // Print line, highlighting matches
            const char *p = line;
            int plen = strlen(pattern);
            while (*p) {
                int pos = strstr_pos(p, pattern);
                if (pos >= 0) {
                    vga_set_color(VGA_WHITE, VGA_BLACK);
                    for (int i = 0; i < pos; i++) vga_putchar(p[i]);
                    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
                    for (int i = 0; i < plen; i++) vga_putchar(pattern[i]);
                    p += pos + plen;
                } else {
                    vga_set_color(VGA_WHITE, VGA_BLACK);
                    vga_print(p);
                    break;
                }
            }
            vga_putchar('\n');
            matches++;
        }

        *eol = saved;
        if (saved == '\n') eol++;
        line = eol;
        line_num++;
    }

    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print_dec(matches);
    vga_print(" match(es)\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

// Simple recursive descent calculator
static const char *calc_ptr;
static int calc_error;

static int calc_expr(void);

static int calc_number(void) {
    while (*calc_ptr == ' ') calc_ptr++;
    int neg = 0;
    if (*calc_ptr == '-') { neg = 1; calc_ptr++; }
    if (*calc_ptr == '(') {
        calc_ptr++;
        int val = calc_expr();
        if (*calc_ptr == ')') calc_ptr++;
        return neg ? -val : val;
    }
    if (*calc_ptr < '0' || *calc_ptr > '9') { calc_error = 1; return 0; }
    int val = 0;
    while (*calc_ptr >= '0' && *calc_ptr <= '9') {
        val = val * 10 + (*calc_ptr - '0');
        calc_ptr++;
    }
    return neg ? -val : val;
}

static int calc_term(void) {
    int val = calc_number();
    while (!calc_error) {
        while (*calc_ptr == ' ') calc_ptr++;
        if (*calc_ptr == '*') { calc_ptr++; val *= calc_number(); }
        else if (*calc_ptr == '/') {
            calc_ptr++;
            int d = calc_number();
            if (d == 0) { calc_error = 1; return 0; }
            val /= d;
        }
        else if (*calc_ptr == '%') {
            calc_ptr++;
            int d = calc_number();
            if (d == 0) { calc_error = 1; return 0; }
            val %= d;
        }
        else break;
    }
    return val;
}

static int calc_expr(void) {
    int val = calc_term();
    while (!calc_error) {
        while (*calc_ptr == ' ') calc_ptr++;
        if (*calc_ptr == '+') { calc_ptr++; val += calc_term(); }
        else if (*calc_ptr == '-') { calc_ptr++; val -= calc_term(); }
        else break;
    }
    return val;
}

static void cmd_calc(const char *args) {
    while (*args == ' ') args++;
    if (strlen(args) == 0) {
        vga_print("Usage: calc <expression>\n");
        return;
    }
    calc_ptr = args;
    calc_error = 0;
    int result = calc_expr();
    if (calc_error) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("  Error: invalid expression\n");
    } else {
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print("  = ");
        if (result < 0) { vga_putchar('-'); result = -result; }
        vga_print_dec(result);
        vga_print("\n");
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_env(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Environment Variables ===\n");
    int count = env_count();
    for (int i = 0; i < count; i++) {
        const char *key, *val;
        env_get_pair(i, &key, &val);
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        vga_print("  ");
        vga_print(key);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print("=");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print(val);
        vga_print("\n");
    }
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print_dec(count);
    vga_print(" variable(s)\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_set(const char *args) {
    while (*args == ' ') args++;
    // Parse KEY=VALUE
    char key[ENV_KEY_SIZE];
    int ki = 0;
    while (*args && *args != '=' && ki < ENV_KEY_SIZE - 1) key[ki++] = *args++;
    key[ki] = '\0';
    if (*args != '=' || ki == 0) {
        vga_print("Usage: set KEY=VALUE\n");
        return;
    }
    args++; // skip =
    if (env_set(key, args) == 0) {
        vga_print("  ");
        vga_print(key);
        vga_print("=");
        vga_print(args);
        vga_print("\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("  Environment full.\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    }
}

static void cmd_unset(const char *args) {
    while (*args == ' ') args++;
    if (env_unset(args) == 0) {
        vga_print("  Removed: ");
        vga_print(args);
        vga_print("\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("  Variable not found: ");
        vga_print(args);
        vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    }
}

static void editor_wait_line(void) {
    editor_line_ready = 0;
    while (!editor_line_ready && !ctrl_c_pressed) {
        __asm__ volatile("hlt");
    }
}

static void cmd_edit(const char *args) {
    while (*args == ' ') args++;
    if (strlen(args) == 0) {
        vga_print("Usage: edit <filename>\n");
        return;
    }

    char content[RAMFS_MAX_FILESIZE];
    int content_len = 0;
    if (ramfs_exists(args)) {
        content_len = ramfs_read(args, content, RAMFS_MAX_FILESIZE - 1);
        if (content_len < 0) content_len = 0;
    }
    content[content_len] = '\0';

    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("=== Editor: ");
    vga_print(args);
    vga_print(" ===\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("  Type text. Commands: :w save, :q quit, :wq save+quit\n");

    if (content_len > 0) {
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print(content);
        if (content[content_len - 1] != '\n') vga_print("\n");
    }

    editor_mode = 1;
    ctrl_c_pressed = 0;

    while (!ctrl_c_pressed) {
        vga_set_color(VGA_DARK_GREY, VGA_BLACK);
        vga_print("> ");
        vga_set_color(VGA_WHITE, VGA_BLACK);

        editor_wait_line();
        if (ctrl_c_pressed) break;

        if (strcmp(editor_line_buf, ":q") == 0) {
            break;
        } else if (strcmp(editor_line_buf, ":w") == 0) {
            ramfs_write(args, content, content_len);
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            vga_print("  Saved ");
            vga_print_dec(content_len);
            vga_print(" bytes\n");
            continue;
        } else if (strcmp(editor_line_buf, ":wq") == 0) {
            ramfs_write(args, content, content_len);
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            vga_print("  Saved ");
            vga_print_dec(content_len);
            vga_print(" bytes\n");
            break;
        }

        int line_len = strlen(editor_line_buf);
        if (content_len + line_len + 1 < RAMFS_MAX_FILESIZE) {
            memcpy(content + content_len, editor_line_buf, line_len);
            content_len += line_len;
            content[content_len++] = '\n';
            content[content_len] = '\0';
        } else {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_print("  File full!\n");
        }
    }

    editor_mode = 0;
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_wc(const char *args) {
    while (*args == ' ') args++;
    if (!ramfs_exists(args)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: "); vga_print(args); vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }
    char buf[RAMFS_MAX_FILESIZE + 1];
    int n = ramfs_read(args, buf, RAMFS_MAX_FILESIZE);
    if (n <= 0) { vga_print("  0 0 0\n"); return; }
    buf[n] = '\0';
    int lines = 0, words = 0, in_word = 0;
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\n') lines++;
        if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("  ");
    vga_print_dec(lines); vga_print(" lines, ");
    vga_print_dec(words); vga_print(" words, ");
    vga_print_dec(n); vga_print(" bytes\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_head(const char *args) {
    while (*args == ' ') args++;
    if (!ramfs_exists(args)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: "); vga_print(args); vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }
    char buf[RAMFS_MAX_FILESIZE + 1];
    int n = ramfs_read(args, buf, RAMFS_MAX_FILESIZE);
    if (n <= 0) return;
    buf[n] = '\0';
    vga_set_color(VGA_WHITE, VGA_BLACK);
    int lines = 0;
    for (int i = 0; i < n && lines < 10; i++) {
        vga_putchar(buf[i]);
        if (buf[i] == '\n') lines++;
    }
    if (lines == 0 || buf[n - 1] != '\n') vga_putchar('\n');
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_tail(const char *args) {
    while (*args == ' ') args++;
    if (!ramfs_exists(args)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: "); vga_print(args); vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }
    char buf[RAMFS_MAX_FILESIZE + 1];
    int n = ramfs_read(args, buf, RAMFS_MAX_FILESIZE);
    if (n <= 0) return;
    buf[n] = '\0';
    // Count total lines
    int total = 0;
    for (int i = 0; i < n; i++) if (buf[i] == '\n') total++;
    int skip = total > 10 ? total - 10 : 0;
    vga_set_color(VGA_WHITE, VGA_BLACK);
    int lines = 0;
    for (int i = 0; i < n; i++) {
        if (lines >= skip) vga_putchar(buf[i]);
        if (buf[i] == '\n') lines++;
    }
    if (buf[n - 1] != '\n') vga_putchar('\n');
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_append(const char *args) {
    while (*args == ' ') args++;
    char filename[RAMFS_MAX_NAME];
    int i = 0;
    while (*args && *args != ' ' && i < RAMFS_MAX_NAME - 1) filename[i++] = *args++;
    filename[i] = '\0';
    if (*args == ' ') args++;
    if (i == 0 || *args == '\0') {
        vga_print("Usage: append <filename> <text>\n");
        return;
    }
    int n = ramfs_append(filename, args, strlen(args));
    if (n >= 0) {
        // Also append a newline
        ramfs_append(filename, "\n", 1);
        vga_print("Appended to "); vga_print(filename); vga_print("\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Append failed.\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    }
}

static void cmd_cp(const char *args) {
    while (*args == ' ') args++;
    char src[RAMFS_MAX_NAME], dst[RAMFS_MAX_NAME];
    int i = 0;
    while (*args && *args != ' ' && i < RAMFS_MAX_NAME - 1) src[i++] = *args++;
    src[i] = '\0';
    while (*args == ' ') args++;
    i = 0;
    while (*args && *args != ' ' && i < RAMFS_MAX_NAME - 1) dst[i++] = *args++;
    dst[i] = '\0';
    if (strlen(src) == 0 || strlen(dst) == 0) {
        vga_print("Usage: cp <source> <dest>\n");
        return;
    }
    if (!ramfs_exists(src)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: "); vga_print(src); vga_print("\n");
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }
    char buf[RAMFS_MAX_FILESIZE];
    int n = ramfs_read(src, buf, RAMFS_MAX_FILESIZE);
    if (n > 0) {
        ramfs_write(dst, buf, n);
        vga_print("Copied "); vga_print(src); vga_print(" -> "); vga_print(dst); vga_print("\n");
    }
}

// ==================== Snake Game ====================

#define SNAKE_MAX 200
#define GAME_W 40
#define GAME_H 20
#define GAME_X0 20  // left offset to center 40-wide board in 80-col screen
#define GAME_Y0 2   // top offset

static void cmd_snake(void) {
    rand_seed(timer_get_ticks());

    int sx[SNAKE_MAX], sy[SNAKE_MAX];
    int slen = 3;
    int dx = 1, dy = 0;
    int score = 0;
    int game_over = 0;

    // Initial snake position (center)
    for (int i = 0; i < slen; i++) {
        sx[i] = GAME_W / 2 - i;
        sy[i] = GAME_H / 2;
    }

    // Place food
    int fx = rand_range(1, GAME_W - 2);
    int fy = rand_range(1, GAME_H - 2);

    // Draw initial board
    vga_clear();
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_put_at(GAME_X0, 0, 'S', VGA_LIGHT_GREEN, VGA_BLACK);
    vga_put_at(GAME_X0+1, 0, 'N', VGA_LIGHT_GREEN, VGA_BLACK);
    vga_put_at(GAME_X0+2, 0, 'A', VGA_LIGHT_GREEN, VGA_BLACK);
    vga_put_at(GAME_X0+3, 0, 'K', VGA_LIGHT_GREEN, VGA_BLACK);
    vga_put_at(GAME_X0+4, 0, 'E', VGA_LIGHT_GREEN, VGA_BLACK);
    vga_put_at(GAME_X0+6, 0, 'W', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+7, 0, 'A', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+8, 0, 'S', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+9, 0, 'D', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+10, 0, '/', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+11, 0, 'A', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+12, 0, 'r', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+13, 0, 'r', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+14, 0, 'o', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+15, 0, 'w', VGA_WHITE, VGA_BLACK);
    vga_put_at(GAME_X0+16, 0, 's', VGA_WHITE, VGA_BLACK);
    // Score label
    vga_put_at(GAME_X0, 1, 'S', VGA_LIGHT_CYAN, VGA_BLACK);
    vga_put_at(GAME_X0+1, 1, 'c', VGA_LIGHT_CYAN, VGA_BLACK);
    vga_put_at(GAME_X0+2, 1, 'o', VGA_LIGHT_CYAN, VGA_BLACK);
    vga_put_at(GAME_X0+3, 1, 'r', VGA_LIGHT_CYAN, VGA_BLACK);
    vga_put_at(GAME_X0+4, 1, 'e', VGA_LIGHT_CYAN, VGA_BLACK);
    vga_put_at(GAME_X0+5, 1, ':', VGA_LIGHT_CYAN, VGA_BLACK);
    vga_put_at(GAME_X0+6, 1, ' ', VGA_LIGHT_CYAN, VGA_BLACK);
    vga_put_at(GAME_X0+7, 1, '0', VGA_WHITE, VGA_BLACK);

    // Draw borders
    for (int x = 0; x < GAME_W; x++) {
        vga_put_at(GAME_X0 + x, GAME_Y0, '#', VGA_DARK_GREY, VGA_BLACK);
        vga_put_at(GAME_X0 + x, GAME_Y0 + GAME_H - 1, '#', VGA_DARK_GREY, VGA_BLACK);
    }
    for (int y = 0; y < GAME_H; y++) {
        vga_put_at(GAME_X0, GAME_Y0 + y, '#', VGA_DARK_GREY, VGA_BLACK);
        vga_put_at(GAME_X0 + GAME_W - 1, GAME_Y0 + y, '#', VGA_DARK_GREY, VGA_BLACK);
    }

    // Draw initial snake
    for (int i = 0; i < slen; i++) {
        vga_put_at(GAME_X0 + sx[i], GAME_Y0 + sy[i],
                   i == 0 ? '@' : 'o', VGA_LIGHT_GREEN, VGA_BLACK);
    }
    // Draw food
    vga_put_at(GAME_X0 + fx, GAME_Y0 + fy, '*', VGA_LIGHT_RED, VGA_BLACK);

    uint32_t last_tick = timer_get_ticks();
    int speed = 8; // ticks between moves (lower = faster)

    while (!game_over) {
        if (ctrl_c_pressed) {
            ctrl_c_pressed = 0;
            break;
        }

        // Poll keyboard for direction changes
        uint8_t status = port_byte_in(0x64);
        if (status & 1) {
            uint8_t sc = port_byte_in(0x60);
            // WASD or arrow keys
            if ((sc == 0x11 || sc == 0x48) && dy != 1)  { dx = 0; dy = -1; } // W / Up
            if ((sc == 0x1F || sc == 0x50) && dy != -1) { dx = 0; dy = 1; }  // S / Down
            if ((sc == 0x1E || sc == 0x4B) && dx != 1)  { dx = -1; dy = 0; } // A / Left
            if ((sc == 0x20 || sc == 0x4D) && dx != -1) { dx = 1; dy = 0; }  // D / Right
            if (sc == 0x01) { game_over = 1; break; } // Escape
        }

        uint32_t now = timer_get_ticks();
        if (now - last_tick < (uint32_t)speed) {
            __asm__ volatile("hlt");
            continue;
        }
        last_tick = now;

        // Erase tail
        int tail_idx = slen - 1;
        vga_put_at(GAME_X0 + sx[tail_idx], GAME_Y0 + sy[tail_idx], ' ', VGA_BLACK, VGA_BLACK);

        // Move body
        for (int i = slen - 1; i > 0; i--) {
            sx[i] = sx[i-1];
            sy[i] = sy[i-1];
        }

        // Move head
        sx[0] += dx;
        sy[0] += dy;

        // Wall collision
        if (sx[0] <= 0 || sx[0] >= GAME_W - 1 || sy[0] <= 0 || sy[0] >= GAME_H - 1) {
            game_over = 1;
            break;
        }

        // Self collision
        for (int i = 1; i < slen; i++) {
            if (sx[0] == sx[i] && sy[0] == sy[i]) {
                game_over = 1;
                break;
            }
        }
        if (game_over) break;

        // Food collision
        if (sx[0] == fx && sy[0] == fy) {
            score++;
            if (slen < SNAKE_MAX) {
                sx[slen] = sx[slen - 1];
                sy[slen] = sy[slen - 1];
                slen++;
            }
            if (speed > 3 && score % 5 == 0) speed--; // speed up every 5 points
            speaker_beep(1000, 50);
            // New food (avoid snake)
            int placed = 0;
            while (!placed) {
                fx = rand_range(1, GAME_W - 2);
                fy = rand_range(1, GAME_H - 2);
                placed = 1;
                for (int i = 0; i < slen; i++) {
                    if (sx[i] == fx && sy[i] == fy) { placed = 0; break; }
                }
            }
            vga_put_at(GAME_X0 + fx, GAME_Y0 + fy, '*', VGA_LIGHT_RED, VGA_BLACK);
            // Update score display
            char sbuf[8];
            itoa(score, sbuf, 10);
            for (int i = 0; sbuf[i]; i++)
                vga_put_at(GAME_X0 + 7 + i, 1, sbuf[i], VGA_WHITE, VGA_BLACK);
        }

        // Draw snake
        vga_put_at(GAME_X0 + sx[0], GAME_Y0 + sy[0], '@', VGA_LIGHT_GREEN, VGA_BLACK);
        if (slen > 1)
            vga_put_at(GAME_X0 + sx[1], GAME_Y0 + sy[1], 'o', VGA_GREEN, VGA_BLACK);
    }

    // Game over message
    speaker_beep(200, 300);
    vga_set_cursor(0, GAME_Y0 + GAME_H + 1);
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_print("  GAME OVER! Score: ");
    vga_print_dec(score);
    vga_print("\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
}

// ==================== Alias ====================

static void cmd_alias(const char *args) {
    // No args: list all aliases
    if (strlen(args) == 0) {
        if (alias_count == 0) {
            vga_print("  No aliases defined.\n");
            return;
        }
        for (int i = 0; i < alias_count; i++) {
            vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
            vga_print("  ");
            vga_print(alias_names[i]);
            vga_set_color(VGA_WHITE, VGA_BLACK);
            vga_print(" = '");
            vga_print(alias_cmds[i]);
            vga_print("'\n");
        }
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        return;
    }

    // Find '=' separator
    const char *eq = args;
    while (*eq && *eq != '=') eq++;
    if (*eq != '=') {
        vga_print("Usage: alias name=command\n");
        return;
    }

    int nlen = eq - args;
    if (nlen == 0 || nlen >= ALIAS_NAME_SIZE) {
        vga_print("Invalid alias name.\n");
        return;
    }

    char name[ALIAS_NAME_SIZE];
    strncpy(name, args, nlen);
    name[nlen] = '\0';
    const char *cmd = eq + 1;

    // Update existing alias
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(alias_names[i], name) == 0) {
            strncpy(alias_cmds[i], cmd, ALIAS_CMD_SIZE - 1);
            alias_cmds[i][ALIAS_CMD_SIZE - 1] = '\0';
            return;
        }
    }

    // Add new
    if (alias_count >= MAX_ALIASES) {
        vga_print("Alias table full.\n");
        return;
    }
    strncpy(alias_names[alias_count], name, ALIAS_NAME_SIZE - 1);
    alias_names[alias_count][ALIAS_NAME_SIZE - 1] = '\0';
    strncpy(alias_cmds[alias_count], cmd, ALIAS_CMD_SIZE - 1);
    alias_cmds[alias_count][ALIAS_CMD_SIZE - 1] = '\0';
    alias_count++;
}

static void cmd_unalias(const char *args) {
    while (*args == ' ') args++;
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(alias_names[i], args) == 0) {
            // Shift remaining
            for (int j = i; j < alias_count - 1; j++) {
                strcpy(alias_names[j], alias_names[j+1]);
                strcpy(alias_cmds[j], alias_cmds[j+1]);
            }
            alias_count--;
            return;
        }
    }
    vga_print("Alias not found: "); vga_print(args); vga_print("\n");
}
