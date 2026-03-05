#include "env.h"
#include "../libc/string.h"

static char keys[ENV_MAX_VARS][ENV_KEY_SIZE];
static char values[ENV_MAX_VARS][ENV_VAL_SIZE];
static int var_count = 0;

void env_init(void) {
    var_count = 0;
    memset(keys, 0, sizeof(keys));
    memset(values, 0, sizeof(values));

    env_set("OS", "NimrodOS");
    env_set("VERSION", "0.1.0");
    env_set("ARCH", "x86");
    env_set("SHELL", "nimrodsh");
    env_set("USER", "root");
    env_set("HOME", "/");
}

int env_set(const char *key, const char *value) {
    // Update existing
    for (int i = 0; i < var_count; i++) {
        if (strcmp(keys[i], key) == 0) {
            strncpy(values[i], value, ENV_VAL_SIZE - 1);
            values[i][ENV_VAL_SIZE - 1] = '\0';
            return 0;
        }
    }
    // Add new
    if (var_count >= ENV_MAX_VARS) return -1;
    strncpy(keys[var_count], key, ENV_KEY_SIZE - 1);
    keys[var_count][ENV_KEY_SIZE - 1] = '\0';
    strncpy(values[var_count], value, ENV_VAL_SIZE - 1);
    values[var_count][ENV_VAL_SIZE - 1] = '\0';
    var_count++;
    return 0;
}

const char *env_get(const char *key) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(keys[i], key) == 0) return values[i];
    }
    return 0;
}

int env_unset(const char *key) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(keys[i], key) == 0) {
            // Shift remaining vars down
            for (int j = i; j < var_count - 1; j++) {
                strcpy(keys[j], keys[j + 1]);
                strcpy(values[j], values[j + 1]);
            }
            var_count--;
            return 0;
        }
    }
    return -1;
}

int env_count(void) {
    return var_count;
}

void env_get_pair(int index, const char **key, const char **value) {
    if (index >= 0 && index < var_count) {
        *key = keys[index];
        *value = values[index];
    }
}
