#pragma once

#include <stdint.h>

int net_init(void);
void net_info(void);
int net_send(const void *data, uint32_t length);
const uint8_t *net_get_mac(void);
void net_poll(void);
void net_tick(void);
void net_dump_regs(void);

