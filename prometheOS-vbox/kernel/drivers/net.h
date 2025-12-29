#pragma once
#include <stdint.h>

void net_stack_init(const uint8_t* mac);
void net_stack_handle_frame(const uint8_t* data, uint16_t len);
void net_stack_tick(void);