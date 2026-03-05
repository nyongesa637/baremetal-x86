#ifndef NET_H
#define NET_H

#include <stdint.h>

// Byte order helpers
static inline uint16_t htons(uint16_t v) { return (v >> 8) | (v << 8); }
static inline uint16_t ntohs(uint16_t v) { return htons(v); }
static inline uint32_t htonl(uint32_t v) {
    return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
           ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

// MAC address
#define MAC_LEN 6
#define MAC_BROADCAST ((uint8_t[]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF})

// Ethernet
#define ETH_HEADER_LEN 14
#define ETH_TYPE_ARP   0x0806
#define ETH_TYPE_IP    0x0800

typedef struct __attribute__((packed)) {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;
} eth_header_t;

// ARP
#define ARP_HW_ETHER   1
#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

typedef struct __attribute__((packed)) {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} arp_packet_t;

// IPv4
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

typedef struct __attribute__((packed)) {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} ip_header_t;

// ICMP
#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} icmp_header_t;

// UDP
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

// DNS
#define DNS_PORT 53
#define DNS_FLAG_QR     0x8000
#define DNS_FLAG_RD     0x0100
#define DNS_FLAG_RA     0x0080
#define DNS_TYPE_A      1
#define DNS_CLASS_IN    1

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

// ARP cache
#define ARP_CACHE_SIZE 16

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    uint8_t  valid;
} arp_entry_t;

// Network config (hardcoded for QEMU user-mode networking)
#define NET_IP          0x0A00020F  // 10.0.2.15
#define NET_GATEWAY     0x0A000202  // 10.0.2.2
#define NET_DNS         0x0A000203  // 10.0.2.3
#define NET_SUBNET_MASK 0xFFFFFF00  // 255.255.255.0

// Network API
void net_init(void);
void net_handle_packet(const void *data, uint16_t len);
int net_send_udp(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                 const void *data, uint16_t len);
int net_send_icmp_echo(uint32_t dst_ip, uint16_t id, uint16_t seq);
int net_dns_resolve(const char *hostname, uint32_t *result_ip);
void net_format_ip(uint32_t ip, char *buf);
void net_format_mac(const uint8_t *mac, char *buf);
uint32_t net_get_ip(void);
uint8_t *net_get_mac(void);
int net_is_up(void);

// Ping state
typedef struct {
    uint32_t target_ip;
    uint16_t id;
    uint16_t seq;
    uint32_t send_tick;
    uint8_t  got_reply;
    uint32_t rtt_ticks;
} ping_state_t;

ping_state_t *net_get_ping_state(void);

#endif
