#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

#define RX_BUF_SIZE 8192
#define RX_BUF_PAD  16
#define RX_BUF_WRAP 1500
#define RX_BUF_TOTAL (RX_BUF_SIZE + RX_BUF_PAD + RX_BUF_WRAP)

#define TX_BUF_SIZE 1536
#define TX_BUF_COUNT 4

int rtl8139_init(void);
int rtl8139_send(const void *data, uint16_t len);
void rtl8139_get_mac(uint8_t *mac);
int rtl8139_available(void);

#endif
