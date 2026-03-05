#ifndef KLOG_H
#define KLOG_H

#include <stdint.h>

void klog_init(void);
void klog(const char *msg);
void klog_hex(const char *label, uint32_t value);
void klog_ok(const char *msg);
void klog_warn(const char *msg);
void klog_err(const char *msg);

#endif
