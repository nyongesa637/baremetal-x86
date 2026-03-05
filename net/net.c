#include "net.h"
#include "../drivers/rtl8139.h"
#include "../drivers/e1000.h"
#include "../drivers/timer.h"
#include "../drivers/keyboard.h"
#include "../kernel/klog.h"
#include "../libc/string.h"

// Which NIC driver is active
#define NIC_NONE    0
#define NIC_RTL8139 1
#define NIC_E1000   2
static int active_nic = NIC_NONE;

static uint8_t our_mac[6];
static uint32_t our_ip = NET_IP;
static arp_entry_t arp_cache[ARP_CACHE_SIZE];
static int net_up = 0;
static ping_state_t ping_state;

// DNS state
static uint32_t dns_result_ip = 0;
static uint8_t dns_got_reply = 0;
static uint16_t dns_query_id = 0;

static uint16_t ip_id_counter = 1;

// Packet buffer for building outgoing packets
static uint8_t pkt_buf[1536];

void net_init(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
    memset(&ping_state, 0, sizeof(ping_state));

    // Try RTL8139 first (QEMU default when specified)
    if (rtl8139_init() == 0) {
        active_nic = NIC_RTL8139;
        rtl8139_get_mac(our_mac);
        klog("  NIC driver: RTL8139\n");
    }
    // Then try E1000 (QEMU default + real Intel hardware)
    else if (e1000_init() == 0) {
        active_nic = NIC_E1000;
        e1000_get_mac(our_mac);
        klog("  NIC driver: E1000\n");
    }
    else {
        klog_warn("Network: no supported NIC found");
        return;
    }

    net_up = 1;

    char mac_str[24];
    net_format_mac(our_mac, mac_str);
    klog("  MAC: ");
    klog(mac_str);
    klog("\n");

    char ip_str[16];
    net_format_ip(htonl(our_ip), ip_str);
    klog("  IP:  ");
    klog(ip_str);
    klog("\n");
}

int net_is_up(void) { return net_up; }
uint32_t net_get_ip(void) { return our_ip; }
uint8_t *net_get_mac(void) { return our_mac; }
ping_state_t *net_get_ping_state(void) { return &ping_state; }

void net_format_ip(uint32_t ip, char *buf) {
    // ip is in network byte order here
    uint8_t *b = (uint8_t *)&ip;
    char tmp[4];
    buf[0] = '\0';
    for (int i = 0; i < 4; i++) {
        itoa(b[i], tmp, 10);
        int len = strlen(buf);
        strcpy(buf + len, tmp);
        if (i < 3) {
            len = strlen(buf);
            buf[len] = '.';
            buf[len + 1] = '\0';
        }
    }
}

void net_format_mac(const uint8_t *mac, char *buf) {
    static const char hex[] = "0123456789AB";
    for (int i = 0; i < 6; i++) {
        buf[i * 3] = hex[(mac[i] >> 4) & 0xF];
        buf[i * 3 + 1] = hex[mac[i] & 0xF];
        buf[i * 3 + 2] = (i < 5) ? ':' : '\0';
    }
}

// Checksum calculation
static uint16_t checksum(const void *data, uint16_t len) {
    const uint16_t *ptr = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t *)ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum;
}

// ARP cache operations
static arp_entry_t *arp_lookup(uint32_t ip) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            return &arp_cache[i];
        }
    }
    return 0;
}

static void arp_update(uint32_t ip, const uint8_t *mac) {
    // Update existing entry
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(arp_cache[i].mac, mac, 6);
            return;
        }
    }
    // Add new entry
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = 1;
            return;
        }
    }
    // Cache full, overwrite first
    arp_cache[0].ip = ip;
    memcpy(arp_cache[0].mac, mac, 6);
    arp_cache[0].valid = 1;
}

// Send raw ethernet frame
static int eth_send(const uint8_t *dst_mac, uint16_t type, const void *payload, uint16_t len) {
    if ((uint32_t)(ETH_HEADER_LEN + len) > sizeof(pkt_buf)) return -1;

    eth_header_t *eth = (eth_header_t *)pkt_buf;
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, our_mac, 6);
    eth->type = htons(type);
    memcpy(pkt_buf + ETH_HEADER_LEN, payload, len);

    // Pad to minimum 60 bytes
    uint16_t total = ETH_HEADER_LEN + len;
    if (total < 60) {
        memset(pkt_buf + total, 0, 60 - total);
        total = 60;
    }

    if (active_nic == NIC_RTL8139)
        return rtl8139_send(pkt_buf, total);
    else if (active_nic == NIC_E1000)
        return e1000_send(pkt_buf, total);
    return -1;
}

// Send ARP request
static void arp_send_request(uint32_t target_ip) {
    arp_packet_t arp;
    arp.hw_type = htons(ARP_HW_ETHER);
    arp.proto_type = htons(ETH_TYPE_IP);
    arp.hw_len = 6;
    arp.proto_len = 4;
    arp.opcode = htons(ARP_OP_REQUEST);
    memcpy(arp.sender_mac, our_mac, 6);
    arp.sender_ip = htonl(our_ip);
    memset(arp.target_mac, 0, 6);
    arp.target_ip = htonl(target_ip);

    eth_send(MAC_BROADCAST, ETH_TYPE_ARP, &arp, sizeof(arp));
}

// Send ARP reply
static void arp_send_reply(uint32_t target_ip, const uint8_t *target_mac) {
    arp_packet_t arp;
    arp.hw_type = htons(ARP_HW_ETHER);
    arp.proto_type = htons(ETH_TYPE_IP);
    arp.hw_len = 6;
    arp.proto_len = 4;
    arp.opcode = htons(ARP_OP_REPLY);
    memcpy(arp.sender_mac, our_mac, 6);
    arp.sender_ip = htonl(our_ip);
    memcpy(arp.target_mac, target_mac, 6);
    arp.target_ip = htonl(target_ip);

    eth_send(target_mac, ETH_TYPE_ARP, &arp, sizeof(arp));
}

// Resolve IP to MAC (blocking with timeout)
static int arp_resolve(uint32_t ip, uint8_t *mac_out) {
    // Check if destination is on local network
    uint32_t resolve_ip = ip;
    if ((ip & NET_SUBNET_MASK) != (our_ip & NET_SUBNET_MASK)) {
        resolve_ip = NET_GATEWAY; // Route through gateway
    }

    arp_entry_t *entry = arp_lookup(resolve_ip);
    if (entry) {
        memcpy(mac_out, entry->mac, 6);
        return 0;
    }

    // Send ARP request and wait
    for (int attempt = 0; attempt < 3; attempt++) {
        arp_send_request(resolve_ip);

        uint32_t start = timer_get_ticks();
        while (timer_get_ticks() - start < 100) {
            if (ctrl_c_pressed) return -1;
            entry = arp_lookup(resolve_ip);
            if (entry) {
                memcpy(mac_out, entry->mac, 6);
                return 0;
            }
            __asm__ volatile("hlt");
        }
    }

    return -1; // Failed to resolve
}

// Send IP packet
static int ip_send(uint32_t dst_ip, uint8_t protocol, const void *payload, uint16_t payload_len) {
    uint8_t ip_pkt[1500];
    ip_header_t *ip = (ip_header_t *)ip_pkt;

    ip->ver_ihl = 0x45; // IPv4, 20-byte header
    ip->tos = 0;
    ip->total_len = htons(20 + payload_len);
    ip->id = htons(ip_id_counter++);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = htonl(our_ip);
    ip->dst_ip = htonl(dst_ip);
    ip->checksum = checksum(ip, 20);

    memcpy(ip_pkt + 20, payload, payload_len);

    uint8_t dst_mac[6];
    if (arp_resolve(dst_ip, dst_mac) < 0) {
        klog_warn("NET: ARP resolve failed");
        return -1;
    }

    return eth_send(dst_mac, ETH_TYPE_IP, ip_pkt, 20 + payload_len);
}

// Handle incoming ARP
static void handle_arp(const arp_packet_t *arp) {
    if (ntohs(arp->hw_type) != ARP_HW_ETHER) return;
    if (ntohs(arp->proto_type) != ETH_TYPE_IP) return;

    uint32_t sender_ip = ntohl(arp->sender_ip);
    uint32_t target_ip = ntohl(arp->target_ip);

    // Always update cache with sender info
    arp_update(sender_ip, arp->sender_mac);

    if (ntohs(arp->opcode) == ARP_OP_REQUEST && target_ip == our_ip) {
        arp_send_reply(sender_ip, arp->sender_mac);
    }
}

// Handle incoming ICMP
static void handle_icmp(const ip_header_t *ip, const void *payload, uint16_t len) {
    if (len < sizeof(icmp_header_t)) return;
    const icmp_header_t *icmp = (const icmp_header_t *)payload;

    if (icmp->type == ICMP_ECHO_REQUEST) {
        // Reply to ping
        uint8_t reply[1500];
        icmp_header_t *ricmp = (icmp_header_t *)reply;
        ricmp->type = ICMP_ECHO_REPLY;
        ricmp->code = 0;
        ricmp->id = icmp->id;
        ricmp->sequence = icmp->sequence;
        ricmp->checksum = 0;
        if (len > sizeof(icmp_header_t)) {
            memcpy(reply + sizeof(icmp_header_t),
                   (const uint8_t *)payload + sizeof(icmp_header_t),
                   len - sizeof(icmp_header_t));
        }
        ricmp->checksum = checksum(reply, len);
        ip_send(ntohl(ip->src_ip), IP_PROTO_ICMP, reply, len);
    } else if (icmp->type == ICMP_ECHO_REPLY) {
        if (ntohs(icmp->id) == ping_state.id && ntohs(icmp->sequence) == ping_state.seq) {
            ping_state.got_reply = 1;
            ping_state.rtt_ticks = timer_get_ticks() - ping_state.send_tick;
        }
    }
}

// Handle incoming UDP
static void handle_udp(const ip_header_t *ip, const void *payload, uint16_t len) {
    (void)ip;
    if (len < sizeof(udp_header_t)) return;
    const udp_header_t *udp = (const udp_header_t *)payload;
    uint16_t src_port = ntohs(udp->src_port);
    uint16_t data_len = ntohs(udp->length) - sizeof(udp_header_t);
    const uint8_t *data = (const uint8_t *)payload + sizeof(udp_header_t);

    if (src_port == DNS_PORT && data_len >= sizeof(dns_header_t)) {
        const dns_header_t *dns = (const dns_header_t *)data;
        if (ntohs(dns->id) == dns_query_id && (ntohs(dns->flags) & DNS_FLAG_QR)) {
            uint16_t ancount = ntohs(dns->ancount);
            if (ancount > 0) {
                // Skip question section
                const uint8_t *ptr = data + sizeof(dns_header_t);
                const uint8_t *end = data + data_len;
                // Skip QNAME
                while (ptr < end && *ptr != 0) {
                    if ((*ptr & 0xC0) == 0xC0) { ptr += 2; goto qskip; }
                    ptr += 1 + *ptr;
                }
                ptr++; // null terminator
                ptr += 4; // QTYPE + QCLASS
                qskip:

                // Parse first answer
                for (uint16_t i = 0; i < ancount && ptr + 12 <= end; i++) {
                    // Skip NAME (may be compressed)
                    if ((*ptr & 0xC0) == 0xC0) {
                        ptr += 2;
                    } else {
                        while (ptr < end && *ptr != 0) ptr += 1 + *ptr;
                        ptr++;
                    }
                    if (ptr + 10 > end) break;
                    uint16_t rtype = (ptr[0] << 8) | ptr[1];
                    uint16_t rdlength = (ptr[8] << 8) | ptr[9];
                    ptr += 10;
                    if (rtype == DNS_TYPE_A && rdlength == 4 && ptr + 4 <= end) {
                        dns_result_ip = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
                        dns_got_reply = 1;
                        return;
                    }
                    ptr += rdlength;
                }
            }
        }
    }
}

// Handle incoming IP packet
static void handle_ip(const void *data, uint16_t len) {
    if (len < sizeof(ip_header_t)) return;
    const ip_header_t *ip = (const ip_header_t *)data;

    uint8_t ihl = (ip->ver_ihl & 0x0F) * 4;
    uint16_t total_len = ntohs(ip->total_len);
    if (total_len > len) return;

    const void *payload = (const uint8_t *)data + ihl;
    uint16_t payload_len = total_len - ihl;

    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            handle_icmp(ip, payload, payload_len);
            break;
        case IP_PROTO_UDP:
            handle_udp(ip, payload, payload_len);
            break;
    }
}

// Main packet handler (called from RTL8139 IRQ)
void net_handle_packet(const void *data, uint16_t len) {
    if (len < ETH_HEADER_LEN) return;
    const eth_header_t *eth = (const eth_header_t *)data;
    uint16_t type = ntohs(eth->type);
    const void *payload = (const uint8_t *)data + ETH_HEADER_LEN;
    uint16_t payload_len = len - ETH_HEADER_LEN;

    switch (type) {
        case ETH_TYPE_ARP:
            handle_arp((const arp_packet_t *)payload);
            break;
        case ETH_TYPE_IP:
            handle_ip(payload, payload_len);
            break;
    }
}

// Public API
int net_send_udp(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                 const void *data, uint16_t len) {
    uint8_t udp_pkt[1472];
    udp_header_t *udp = (udp_header_t *)udp_pkt;

    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons(sizeof(udp_header_t) + len);
    udp->checksum = 0; // optional for UDP over IPv4

    memcpy(udp_pkt + sizeof(udp_header_t), data, len);

    return ip_send(dst_ip, IP_PROTO_UDP, udp_pkt, sizeof(udp_header_t) + len);
}

int net_send_icmp_echo(uint32_t dst_ip, uint16_t id, uint16_t seq) {
    uint8_t icmp_pkt[64];
    icmp_header_t *icmp = (icmp_header_t *)icmp_pkt;

    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->id = htons(id);
    icmp->sequence = htons(seq);
    icmp->checksum = 0;

    // Add some payload data
    for (int i = 0; i < 56; i++) {
        icmp_pkt[sizeof(icmp_header_t) + i] = (uint8_t)i;
    }

    icmp->checksum = checksum(icmp_pkt, sizeof(icmp_header_t) + 56);

    ping_state.target_ip = dst_ip;
    ping_state.id = id;
    ping_state.seq = seq;
    ping_state.got_reply = 0;
    ping_state.send_tick = timer_get_ticks();

    return ip_send(dst_ip, IP_PROTO_ICMP, icmp_pkt, sizeof(icmp_header_t) + 56);
}

int net_dns_resolve(const char *hostname, uint32_t *result_ip) {
    // Build DNS query
    uint8_t query[256];
    dns_header_t *dns = (dns_header_t *)query;

    dns_query_id = (uint16_t)(timer_get_ticks() & 0xFFFF);
    dns->id = htons(dns_query_id);
    dns->flags = htons(DNS_FLAG_RD);
    dns->qdcount = htons(1);
    dns->ancount = 0;
    dns->nscount = 0;
    dns->arcount = 0;

    // Encode hostname as DNS QNAME
    uint8_t *ptr = query + sizeof(dns_header_t);
    const char *src = hostname;
    while (*src) {
        uint8_t *len_byte = ptr++;
        uint8_t label_len = 0;
        while (*src && *src != '.') {
            *ptr++ = *src++;
            label_len++;
        }
        *len_byte = label_len;
        if (*src == '.') src++;
    }
    *ptr++ = 0; // null terminator

    // QTYPE = A (1), QCLASS = IN (1)
    *ptr++ = 0; *ptr++ = DNS_TYPE_A;
    *ptr++ = 0; *ptr++ = DNS_CLASS_IN;

    uint16_t query_len = ptr - query;
    dns_got_reply = 0;
    dns_result_ip = 0;

    // Send query to DNS server
    for (int attempt = 0; attempt < 3; attempt++) {
        net_send_udp(NET_DNS, 1053 + attempt, DNS_PORT, query, query_len);

        uint32_t start = timer_get_ticks();
        while (timer_get_ticks() - start < 200) {
            if (ctrl_c_pressed) return -1;
            if (dns_got_reply) {
                *result_ip = dns_result_ip;
                return 0;
            }
            __asm__ volatile("hlt");
        }
    }

    return -1;
}
