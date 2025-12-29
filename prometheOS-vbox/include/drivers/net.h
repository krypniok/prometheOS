#pragma once

#include <stdint.h>

int net_init(void);
void net_info(void);
int net_send(const void *data, uint32_t length);
const uint8_t *net_get_mac(void);
void net_poll(void);
void net_tick(void);
void net_dump_regs(void);

// Stack functions
void net_stack_init(const uint8_t* mac);
void net_stack_handle_frame(const uint8_t* data, uint16_t len);
void net_stack_tick(void);

// Helpers
static inline uint32_t net_ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

int net_ping(uint32_t dest_ip, int timeout_ms);
