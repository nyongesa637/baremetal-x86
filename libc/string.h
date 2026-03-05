#ifndef STRING_H
#define STRING_H

#include <stdint.h>

int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, int n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int n);
void *memset(void *ptr, int value, uint32_t size);
void *memcpy(void *dest, const void *src, uint32_t size);
int starts_with(const char *str, const char *prefix);
void itoa(int value, char *buf, int base);

#endif
