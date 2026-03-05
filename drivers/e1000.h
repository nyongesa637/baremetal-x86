#ifndef E1000_H
#define E1000_H

#include <stdint.h>

// Intel vendor ID and common E1000 device IDs
#define E1000_VENDOR_ID  0x8086

// Device IDs for various Intel NICs
#define E1000_DEV_82540EM    0x100E  // QEMU default
#define E1000_DEV_82545EM    0x100F
#define E1000_DEV_82574L     0x10D3  // Common server/desktop NIC
#define E1000_DEV_I217LM     0x153A  // Haswell era laptops
#define E1000_DEV_I217V      0x153B
#define E1000_DEV_I218LM     0x155A  // Broadwell era
#define E1000_DEV_I218V      0x1559
#define E1000_DEV_I219LM     0x156F  // Skylake era
#define E1000_DEV_I219V      0x1570
#define E1000_DEV_I219LM2    0x15B7  // Kaby Lake
#define E1000_DEV_I219V2     0x15B8
#define E1000_DEV_I219LM3    0x15BB  // Coffee Lake
#define E1000_DEV_I219V3     0x15BC
#define E1000_DEV_I219LM10   0x0D4E  // Comet Lake
#define E1000_DEV_I219V10    0x0D4F
#define E1000_DEV_I219LM12   0x0D53  // Tiger Lake
#define E1000_DEV_I219V12    0x0D55
#define E1000_DEV_I225V      0x15F3  // Modern desktop
#define E1000_DEV_I226V      0x125C  // Alder Lake+

int e1000_init(void);
int e1000_send(const void *data, uint16_t len);
void e1000_get_mac(uint8_t *mac);
int e1000_available(void);

#endif
