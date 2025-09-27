#pragma once

#include <stdint.h>
#include <stddef.h>

void net_stack_init(const uint8_t mac[6]);
void net_stack_handle_frame(const uint8_t *frame, uint16_t length);
void net_stack_tick(void);

uint32_t net_ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
int net_parse_ipv4(const char *str, uint32_t *out_ip);

int net_tcp_connect(uint32_t ip, uint16_t port, int timeout_ms);
int net_tcp_connected(void);
int net_tcp_send(const uint8_t *data, uint16_t length);
int net_tcp_recv(uint8_t *buffer, uint16_t max_length, int timeout_ms);
void net_tcp_close(void);

int net_ping(uint32_t ip, int timeout_ms);

