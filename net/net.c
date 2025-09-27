#include "net.h"

#include "../drivers/net.h"
#include "../drivers/display.h"
#include "../drivers/debug.h"
#include "../stdlibs/string.h"
#include "../stdlibs/memory.h"
#include "../kernel/time.h"
#include "../kernel/thread.h"

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

#define TCP_STATE_CLOSED      0
#define TCP_STATE_SYN_SENT    1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_FIN_WAIT    3
#define TCP_STATE_CLOSE_WAIT  4

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

#pragma pack(push, 1)
typedef struct {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;
} ethernet_header_t;

typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} arp_packet_t;

typedef struct {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} ipv4_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_number;
    uint32_t ack_number;
    uint8_t data_offset_reserved;
    uint8_t flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
} tcp_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t mac[6];
    uint32_t ip_addr;
    uint32_t gateway;
    uint32_t netmask;

    uint32_t remote_ip;
    uint8_t remote_mac[6];
    uint8_t remote_mac_valid;
    uint16_t remote_port;
    uint16_t local_port;

    uint32_t send_next;
    uint32_t send_una;
    uint32_t recv_next;

    uint16_t ip_id;
    uint8_t state;

    uint64_t last_syn_ms;
    uint64_t last_send_ms;

    uint8_t rx_buffer[4096];
    uint16_t rx_len;

    uint8_t tx_last[1500];
    uint16_t tx_last_len;
    uint32_t tx_last_seq;
    uint8_t awaiting_ack;
} tcp_context_t;

static tcp_context_t g_tcp;

typedef struct {
    uint8_t valid;
    uint32_t ip;
    uint8_t mac[6];
    uint64_t timestamp_ms;
} arp_cache_entry_t;

static arp_cache_entry_t g_arp_cache;

typedef struct {
    uint8_t active;
    uint8_t success;
    uint16_t id;
    uint16_t seq;
    uint32_t target_ip;
    uint64_t send_time_ms;
    uint64_t rtt_ms;
} icmp_state_t;

static icmp_state_t g_icmp = { .id = 0x4242 };

static inline uint16_t swap16(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}

static inline uint32_t swap32(uint32_t v) {
    return ((v & 0xFFu) << 24) | ((v & 0xFF00u) << 8) | ((v & 0xFF0000u) >> 8) | ((v >> 24) & 0xFFu);
}

static inline uint16_t htons16(uint16_t v) { return swap16(v); }
static inline uint16_t ntohs16(uint16_t v) { return swap16(v); }
static inline uint32_t htonl32(uint32_t v) { return swap32(v); }
static inline uint32_t ntohl32(uint32_t v) { return swap32(v); }

static uint16_t checksum16(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += (uint32_t)(bytes[0] << 8 | bytes[1]);
        bytes += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint32_t)(bytes[0] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static uint16_t tcp_checksum(const ipv4_header_t* ip, const tcp_header_t* tcp, const uint8_t* payload, uint16_t payload_len) {
    uint32_t sum = 0;
    uint16_t tcp_len = (uint16_t)(sizeof(tcp_header_t) + payload_len);

    uint32_t src = ntohl32(ip->src_ip);
    uint32_t dst = ntohl32(ip->dst_ip);
    sum += (src >> 16) & 0xFFFFu;
    sum += src & 0xFFFFu;
    sum += (dst >> 16) & 0xFFFFu;
    sum += dst & 0xFFFFu;
    sum += (uint16_t)ip->protocol;
    sum += tcp_len;

    const uint8_t* data = (const uint8_t*)tcp;
    uint16_t len = tcp_len;
    while (len > 1) {
        sum += (uint32_t)(data[0] << 8 | data[1]);
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint32_t)(data[0] << 8);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static void memmove_local(uint8_t *dst, const uint8_t *src, uint16_t len) {
    if (dst == src || len == 0) {
        return;
    }
    if (dst < src) {
        for (uint16_t i = 0; i < len; ++i) {
            dst[i] = src[i];
        }
    } else {
        for (uint16_t i = len; i > 0; --i) {
            dst[i - 1] = src[i - 1];
        }
    }
}

uint32_t net_ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

int net_parse_ipv4(const char *str, uint32_t *out_ip) {
    if (!str || !out_ip) {
        return -1;
    }
    uint32_t parts[4] = {0};
    int part = 0;
    uint32_t acc = 0;
    for (size_t i = 0; str[i]; ++i) {
        char ch = str[i];
        if (ch >= '0' && ch <= '9') {
            acc = acc * 10 + (uint32_t)(ch - '0');
            if (acc > 255) {
                return -1;
            }
        } else if (ch == '.') {
            if (part >= 4) {
                return -1;
            }
            parts[part++] = acc;
            acc = 0;
        } else {
            return -1;
        }
    }
    parts[part++] = acc;
    if (part != 4) {
        return -1;
    }
    *out_ip = net_ipv4((uint8_t)parts[0], (uint8_t)parts[1], (uint8_t)parts[2], (uint8_t)parts[3]);
    return 0;
}

static void tcp_reset_state(void) {
    g_tcp.remote_ip = 0;
    g_tcp.remote_port = 0;
    g_tcp.remote_mac_valid = 0;
    g_tcp.rx_len = 0;
    g_tcp.tx_last_len = 0;
    g_tcp.awaiting_ack = 0;
    g_tcp.state = TCP_STATE_CLOSED;
}

static void arp_cache_store(uint32_t ip, const uint8_t mac[6]) {
    g_arp_cache.valid = 1;
    g_arp_cache.ip = ip;
    g_arp_cache.timestamp_ms = millis();
    memcpy(g_arp_cache.mac, mac, 6);
}

static int arp_cache_lookup(uint32_t ip, uint8_t mac_out[6], uint64_t max_age_ms) {
    if (g_arp_cache.valid && g_arp_cache.ip == ip) {
        if ((millis() - g_arp_cache.timestamp_ms) <= max_age_ms) {
            memcpy(mac_out, g_arp_cache.mac, 6);
            return 0;
        }
    }
    return -1;
}

static void send_arp_request(uint32_t target_ip) {
    uint8_t frame[64];
    ethernet_header_t *eth = (ethernet_header_t*)frame;
    memset(eth->dest, 0xFF, 6);
    memcpy(eth->src, g_tcp.mac, 6);
    eth->type = htons16(0x0806);

    arp_packet_t *arp = (arp_packet_t*)(frame + sizeof(ethernet_header_t));
    arp->hw_type = htons16(1);
    arp->proto_type = htons16(0x0800);
    arp->hw_size = 6;
    arp->proto_size = 4;
    arp->opcode = htons16(ARP_OP_REQUEST);
    memcpy(arp->sender_mac, g_tcp.mac, 6);
    arp->sender_ip = htonl32(g_tcp.ip_addr);
    memset(arp->target_mac, 0x00, 6);
    arp->target_ip = htonl32(target_ip);

    net_send(frame, sizeof(ethernet_header_t) + sizeof(arp_packet_t));
    printf("[net] ARP req %d.%d.%d.%d\n",
           (target_ip >> 24) & 0xFF,
           (target_ip >> 16) & 0xFF,
           (target_ip >> 8) & 0xFF,
           target_ip & 0xFF);
}

static void send_arp_reply(const uint8_t target_mac[6], uint32_t target_ip) {
    uint8_t frame[64];
    ethernet_header_t *eth = (ethernet_header_t*)frame;
    memcpy(eth->dest, target_mac, 6);
    memcpy(eth->src, g_tcp.mac, 6);
    eth->type = htons16(0x0806);

    arp_packet_t *arp = (arp_packet_t*)(frame + sizeof(ethernet_header_t));
    arp->hw_type = htons16(1);
    arp->proto_type = htons16(0x0800);
    arp->hw_size = 6;
    arp->proto_size = 4;
    arp->opcode = htons16(ARP_OP_REPLY);
    memcpy(arp->sender_mac, g_tcp.mac, 6);
    arp->sender_ip = htonl32(g_tcp.ip_addr);
    memcpy(arp->target_mac, target_mac, 6);
    arp->target_ip = htonl32(target_ip);

    net_send(frame, sizeof(ethernet_header_t) + sizeof(arp_packet_t));
}

static int arp_resolve(uint32_t ip, uint8_t mac_out[6], int timeout_ms) {
    if (arp_cache_lookup(ip, mac_out, 10000) == 0) {
        return 0;
    }

    debug_puts("[net] arp_resolve enter\n");
    uint64_t start = millis();
    uint64_t last_request = 0;
    while ((int)((int64_t)(millis() - start)) < timeout_ms) {
        debug_puts("[net] arp loop top\n");
        uint64_t now = millis();
        if (!last_request || (now - last_request) >= 1000) {
            send_arp_request(ip);
            last_request = now;
            debug_puts("[net] arp_resolve waited\n");
        }

        net_poll();
        net_tick();

        if (arp_cache_lookup(ip, mac_out, 10000) == 0) {
            debug_puts("[net] arp resolved cache hit\n");
            return 0;
        }

        thread_yield();
        sleep_us(1000);
    }
    debug_puts("[net] arp_resolve timeout\n");
    return -1;
}

void net_stack_init(const uint8_t mac[6]) {
    if (mac) {
        memcpy(g_tcp.mac, mac, 6);
    }
    g_tcp.ip_addr = net_ipv4(10, 0, 2, 15);
    g_tcp.gateway = net_ipv4(10, 0, 2, 2);
    g_tcp.netmask = net_ipv4(255, 255, 255, 0);
    g_tcp.ip_id = 1;
    tcp_reset_state();
    g_arp_cache.valid = 0;
    g_icmp.active = 0;
}

static void tcp_send_packet(uint8_t flags, uint32_t seq, uint32_t ack, const uint8_t *payload, uint16_t payload_len) {
    if (!g_tcp.remote_mac_valid) {
        return;
    }

    uint8_t frame[1514];
    ethernet_header_t *eth = (ethernet_header_t*)frame;
    memcpy(eth->dest, g_tcp.remote_mac, 6);
    memcpy(eth->src, g_tcp.mac, 6);
    eth->type = htons16(0x0800);

    ipv4_header_t *ip = (ipv4_header_t*)(frame + sizeof(ethernet_header_t));
    tcp_header_t *tcp = (tcp_header_t*)((uint8_t*)ip + sizeof(ipv4_header_t));

    uint16_t total_length = (uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t) + payload_len);

    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons16(total_length);
    ip->identification = htons16(g_tcp.ip_id++);
    ip->flags_fragment = htons16(0x4000);
    ip->ttl = 64;
    ip->protocol = 6;
    ip->checksum = 0;
    ip->src_ip = htonl32(g_tcp.ip_addr);
    ip->dst_ip = htonl32(g_tcp.remote_ip);
    ip->checksum = checksum16(ip, sizeof(ipv4_header_t));

    tcp->src_port = htons16(g_tcp.local_port);
    tcp->dst_port = htons16(g_tcp.remote_port);
    tcp->seq_number = htonl32(seq);
    tcp->ack_number = htonl32(ack);
    tcp->data_offset_reserved = (uint8_t)((sizeof(tcp_header_t) / 4) << 4);
    tcp->flags = flags;
    tcp->window_size = htons16(0x2000);
    tcp->checksum = 0;
    tcp->urgent_pointer = 0;

    if (payload_len) {
        memcpy((uint8_t*)tcp + sizeof(tcp_header_t), payload, payload_len);
    }

    tcp->checksum = tcp_checksum(ip, tcp, payload, payload_len);

    net_send(frame, sizeof(ethernet_header_t) + total_length);
    g_tcp.last_send_ms = millis();
}

static void tcp_send_syn(void) {
    tcp_send_packet(TCP_FLAG_SYN, g_tcp.send_next, 0, 0, 0);
    g_tcp.send_next += 1;
    g_tcp.awaiting_ack = 1;
    g_tcp.last_syn_ms = millis();
}

static void tcp_send_ack(void) {
    tcp_send_packet(TCP_FLAG_ACK, g_tcp.send_next, g_tcp.recv_next, 0, 0);
}

static void tcp_handle_segment(const ipv4_header_t *ip, const tcp_header_t *tcp, const uint8_t *payload, uint16_t payload_len) {
    uint16_t dst_port = ntohs16(tcp->dst_port);
    if (dst_port != g_tcp.local_port) {
        return;
    }

    uint16_t src_port = ntohs16(tcp->src_port);
    if (g_tcp.remote_port && src_port != g_tcp.remote_port) {
        return;
    }

    uint32_t seq = ntohl32(tcp->seq_number);
    uint32_t ack = ntohl32(tcp->ack_number);
    uint8_t flags = tcp->flags;

    if (flags & TCP_FLAG_RST) {
        printf("[net] Verbindung von Host reset.\n");
        tcp_reset_state();
        return;
    }

    if (flags & TCP_FLAG_ACK) {
        if (ack > g_tcp.send_una) {
            g_tcp.send_una = ack;
            if (g_tcp.send_una == g_tcp.send_next) {
                g_tcp.awaiting_ack = 0;
                g_tcp.tx_last_len = 0;
            }
        }
    }

    if (g_tcp.state == TCP_STATE_SYN_SENT) {
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK) && ack == g_tcp.send_next) {
            g_tcp.remote_port = src_port;
            g_tcp.recv_next = seq + 1;
            g_tcp.state = TCP_STATE_ESTABLISHED;
            g_tcp.awaiting_ack = 0;
            tcp_send_ack();
            printf("[net] TCP Verbindung hergestellt.\n");
            if (payload_len) {
                uint16_t cp = payload_len;
                if (cp > sizeof(g_tcp.rx_buffer) - g_tcp.rx_len) {
                    cp = (uint16_t)(sizeof(g_tcp.rx_buffer) - g_tcp.rx_len);
                }
                memcpy(g_tcp.rx_buffer + g_tcp.rx_len, payload, cp);
                g_tcp.rx_len += cp;
                g_tcp.recv_next += cp;
                tcp_send_ack();
            }
            return;
        }
    }

    if (g_tcp.state != TCP_STATE_ESTABLISHED && g_tcp.state != TCP_STATE_FIN_WAIT && g_tcp.state != TCP_STATE_CLOSE_WAIT) {
        return;
    }

    if (payload_len && seq == g_tcp.recv_next) {
        uint16_t cp = payload_len;
        if (cp > sizeof(g_tcp.rx_buffer) - g_tcp.rx_len) {
            cp = (uint16_t)(sizeof(g_tcp.rx_buffer) - g_tcp.rx_len);
        }
        if (cp) {
            memcpy(g_tcp.rx_buffer + g_tcp.rx_len, payload, cp);
            g_tcp.rx_len += cp;
        }
        g_tcp.recv_next += payload_len;
        tcp_send_ack();
    }

    if (flags & TCP_FLAG_FIN) {
        g_tcp.recv_next += 1;
        tcp_send_ack();
        g_tcp.state = TCP_STATE_CLOSE_WAIT;
        printf("[net] Remote host hat die Verbindung geschlossen.\n");
    }
}

static void handle_icmp(const ipv4_header_t *ip, const uint8_t *payload, uint16_t length) {
    if (length < 8) {
        return;
    }
    uint32_t dst = ntohl32(ip->dst_ip);
    if (dst != g_tcp.ip_addr) {
        return;
    }
    uint8_t type = payload[0];
    uint8_t code = payload[1];
    if (type == 0 && code == 0) {
        uint16_t identifier = ntohs16(*(const uint16_t*)(payload + 4));
        uint16_t sequence = ntohs16(*(const uint16_t*)(payload + 6));
        if (g_icmp.active && identifier == g_icmp.id && sequence == g_icmp.seq && ntohl32(ip->src_ip) == g_icmp.target_ip) {
            g_icmp.active = 0;
            g_icmp.success = 1;
            g_icmp.rtt_ms = millis() - g_icmp.send_time_ms;
        }
    }
}

static void handle_ipv4(const uint8_t *frame, uint16_t length) {
    if (length < sizeof(ipv4_header_t)) {
        return;
    }
    const ipv4_header_t *ip = (const ipv4_header_t*)frame;
    uint8_t ihl = (uint8_t)((ip->version_ihl & 0x0F) * 4);
    if (ihl < sizeof(ipv4_header_t) || length < ihl) {
        return;
    }
    uint16_t total_length = ntohs16(ip->total_length);
    if (total_length < ihl || length < total_length) {
        return;
    }

    const uint8_t *payload = frame + ihl;
    uint16_t payload_len = (uint16_t)(total_length - ihl);

    if (ip->protocol == 1) {
        handle_icmp(ip, payload, payload_len);
        return;
    }

    if (ip->protocol != 6) {
        return;
    }

    uint32_t dst_ip = ntohl32(ip->dst_ip);
    if (dst_ip != g_tcp.ip_addr) {
        return;
    }
    uint32_t src_ip = ntohl32(ip->src_ip);
    if (g_tcp.remote_ip && src_ip != g_tcp.remote_ip) {
        return;
    }

    if (payload_len < sizeof(tcp_header_t)) {
        return;
    }

    const tcp_header_t *tcp = (const tcp_header_t*)payload;
    uint8_t data_offset = (uint8_t)((tcp->data_offset_reserved >> 4) * 4);
    if (data_offset < sizeof(tcp_header_t) || payload_len < data_offset) {
        return;
    }
    const uint8_t *tcp_payload = payload + data_offset;
    uint16_t tcp_payload_len = (uint16_t)(payload_len - data_offset);

    tcp_handle_segment(ip, tcp, tcp_payload, tcp_payload_len);
}

static void handle_arp(const uint8_t *frame, uint16_t length) {
    if (length < sizeof(arp_packet_t)) {
        return;
    }
    const arp_packet_t *arp = (const arp_packet_t*)frame;
    uint16_t opcode = ntohs16(arp->opcode);
    uint32_t sender_ip = ntohl32(arp->sender_ip);
    uint32_t target_ip = ntohl32(arp->target_ip);

    arp_cache_store(sender_ip, arp->sender_mac);
    printf("[net] ARP got %d.%d.%d.%d -> %02X:%02X:%02X:%02X:%02X:%02X\n",
           (sender_ip >> 24) & 0xFF,
           (sender_ip >> 16) & 0xFF,
           (sender_ip >> 8) & 0xFF,
           sender_ip & 0xFF,
           arp->sender_mac[0], arp->sender_mac[1], arp->sender_mac[2],
           arp->sender_mac[3], arp->sender_mac[4], arp->sender_mac[5]);

    if (opcode == ARP_OP_REQUEST && target_ip == g_tcp.ip_addr) {
        send_arp_reply(arp->sender_mac, sender_ip);
    }
}

void net_stack_handle_frame(const uint8_t *frame, uint16_t length) {
    if (length < sizeof(ethernet_header_t)) {
        return;
    }
    const ethernet_header_t *eth = (const ethernet_header_t*)frame;
    uint16_t type = ntohs16(eth->type);
    const uint8_t *payload = frame + sizeof(ethernet_header_t);
    uint16_t payload_len = (uint16_t)(length - sizeof(ethernet_header_t));

    if (type == 0x0806) {
        handle_arp(payload, payload_len);
    } else if (type == 0x0800) {
        handle_ipv4(payload, payload_len);
    }
}

void net_stack_tick(void) {
    uint64_t now = millis();

    if (g_tcp.state == TCP_STATE_SYN_SENT) {
        if (g_tcp.awaiting_ack && (now - g_tcp.last_syn_ms) >= 1500) {
            tcp_send_syn();
        }
    }

    if (g_tcp.state == TCP_STATE_ESTABLISHED && g_tcp.awaiting_ack && g_tcp.tx_last_len) {
        if ((now - g_tcp.last_send_ms) >= 2000) {
            tcp_send_packet(TCP_FLAG_ACK | TCP_FLAG_PSH, g_tcp.tx_last_seq, g_tcp.recv_next, g_tcp.tx_last, g_tcp.tx_last_len);
        }
    }
}

int net_tcp_connected(void) {
    return g_tcp.state == TCP_STATE_ESTABLISHED;
}

int net_tcp_connect(uint32_t ip, uint16_t port, int timeout_ms) {
    if (!net_get_mac()) {
        return -1;
    }

    if (g_tcp.state != TCP_STATE_CLOSED) {
        net_tcp_close();
    }

    g_tcp.remote_ip = ip;
    g_tcp.remote_port = port;
    g_tcp.remote_mac_valid = 0;
    g_tcp.local_port = (uint16_t)(40000 + (millis() & 0x3FFF));
    g_tcp.send_next = (uint32_t)(micros() & 0x7FFFFFFF);
    g_tcp.send_una = g_tcp.send_next;
    g_tcp.recv_next = 0;
    g_tcp.last_syn_ms = 0;
    g_tcp.awaiting_ack = 0;
    g_tcp.tx_last_len = 0;
    g_tcp.rx_len = 0;

    if (arp_resolve(ip, g_tcp.remote_mac, timeout_ms) != 0) {
        printf("[net] ARP-Aufloesung fehlgeschlagen.\n");
        tcp_reset_state();
        return -1;
    }
    g_tcp.remote_mac_valid = 1;

    g_tcp.state = TCP_STATE_SYN_SENT;
    tcp_send_syn();

    uint64_t start = millis();
    while (g_tcp.state != TCP_STATE_ESTABLISHED) {
        if ((int)((int64_t)(millis() - start)) >= timeout_ms) {
            printf("[net] TCP Verbindung Timeout.\n");
            tcp_reset_state();
            return -1;
        }
        net_poll();
        net_tick();
        thread_yield();
        sleep_us(1000);
    }
    return 0;
}

int net_tcp_send(const uint8_t *data, uint16_t length) {
    if (!data || !length) {
        return 0;
    }
    if (g_tcp.state != TCP_STATE_ESTABLISHED) {
        return -1;
    }

    while (g_tcp.awaiting_ack) {
        net_poll();
        net_tick();
        thread_yield();
        sleep_us(1000);
        if (g_tcp.state != TCP_STATE_ESTABLISHED) {
            return -1;
        }
    }

    uint32_t seq = g_tcp.send_next;
    tcp_send_packet(TCP_FLAG_ACK | TCP_FLAG_PSH, seq, g_tcp.recv_next, data, length);
    g_tcp.send_next += length;
    g_tcp.awaiting_ack = 1;
    g_tcp.tx_last_len = length;
    g_tcp.tx_last_seq = seq;
    if (length <= sizeof(g_tcp.tx_last)) {
        memcpy(g_tcp.tx_last, data, length);
    }
    return length;
}

int net_tcp_recv(uint8_t *buffer, uint16_t max_length, int timeout_ms) {
    if (!buffer || !max_length) {
        return -1;
    }
    if (g_tcp.state != TCP_STATE_ESTABLISHED && g_tcp.state != TCP_STATE_CLOSE_WAIT) {
        return -1;
    }

    uint64_t start = millis();
    while (g_tcp.rx_len == 0) {
        if (timeout_ms >= 0 && (int)((int64_t)(millis() - start)) >= timeout_ms) {
            return 0;
        }
        net_poll();
        net_tick();
        if (g_tcp.state != TCP_STATE_ESTABLISHED && g_tcp.state != TCP_STATE_CLOSE_WAIT) {
            return 0;
        }
        thread_yield();
        sleep_us(1000);
    }

    uint16_t take = g_tcp.rx_len;
    if (take > max_length) {
        take = max_length;
    }
    memcpy(buffer, g_tcp.rx_buffer, take);
    if (take < g_tcp.rx_len) {
        memmove_local(g_tcp.rx_buffer, g_tcp.rx_buffer + take, (uint16_t)(g_tcp.rx_len - take));
    }
    g_tcp.rx_len -= take;
    return take;
}

void net_tcp_close(void) {
    if (g_tcp.state == TCP_STATE_ESTABLISHED || g_tcp.state == TCP_STATE_CLOSE_WAIT) {
        tcp_send_packet(TCP_FLAG_RST | TCP_FLAG_ACK, g_tcp.send_next, g_tcp.recv_next, 0, 0);
    }
    tcp_reset_state();
}

int net_ping(uint32_t ip, int timeout_ms) {
    uint8_t mac[6];
    debug_puts("[net] ping start\n");
    if (arp_resolve(ip, mac, timeout_ms) != 0) {
        debug_puts("[net] ping arp fail\n");
        return -1;
    }

    debug_puts("[net] ping arp ok\n");
    uint8_t frame[1514];
    ethernet_header_t *eth = (ethernet_header_t*)frame;
    memcpy(eth->dest, mac, 6);
    memcpy(eth->src, g_tcp.mac, 6);
    eth->type = htons16(0x0800);

    ipv4_header_t *ip_hdr = (ipv4_header_t*)(frame + sizeof(ethernet_header_t));
    uint8_t *icmp = (uint8_t*)ip_hdr + sizeof(ipv4_header_t);

    uint16_t icmp_len = 8 + 4;
    memset(icmp, 0, icmp_len);
    icmp[0] = 8; // Echo request
    *(uint16_t*)(icmp + 4) = htons16(g_icmp.id);
    g_icmp.seq++;
    *(uint16_t*)(icmp + 6) = htons16(g_icmp.seq);
    *(uint32_t*)(icmp + 8) = htonl32((uint32_t)millis());
    *(uint16_t*)(icmp + 2) = checksum16(icmp, icmp_len);

    uint16_t total_length = (uint16_t)(sizeof(ipv4_header_t) + icmp_len);
    ip_hdr->version_ihl = 0x45;
    ip_hdr->tos = 0;
    ip_hdr->total_length = htons16(total_length);
    ip_hdr->identification = htons16(g_tcp.ip_id++);
    ip_hdr->flags_fragment = htons16(0x4000);
    ip_hdr->ttl = 64;
    ip_hdr->protocol = 1;
    ip_hdr->checksum = 0;
    ip_hdr->src_ip = htonl32(g_tcp.ip_addr);
    ip_hdr->dst_ip = htonl32(ip);
    ip_hdr->checksum = checksum16(ip_hdr, sizeof(ipv4_header_t));

    g_icmp.active = 1;
    g_icmp.success = 0;
    g_icmp.target_ip = ip;
    g_icmp.send_time_ms = millis();

    net_send(frame, sizeof(ethernet_header_t) + total_length);
    debug_puts("[net] ping sent\n");

    uint64_t start = millis();
    while (g_icmp.active) {
        if ((int)((int64_t)(millis() - start)) >= timeout_ms) {
            g_icmp.active = 0;
            break;
        }
        net_poll();
        net_tick();
        thread_yield();
        sleep_us(1000);
    }

    if (g_icmp.success) {
        debug_puts("[net] ping success\n");
        return (int)g_icmp.rtt_ms;
    }
    debug_puts("[net] ping timeout\n");
    return -1;
}
