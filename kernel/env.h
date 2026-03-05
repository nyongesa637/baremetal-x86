#ifndef ENV_H
#define ENV_H

#define ENV_MAX_VARS  32
#define ENV_KEY_SIZE  32
#define ENV_VAL_SIZE  128

void env_init(void);
int env_set(const char *key, const char *value);
const char *env_get(const char *key);
int env_unset(const char *key);
int env_count(void);
void env_get_pair(int index, const char **key, const char **value);

#endif
