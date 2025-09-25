#pragma once

#define delay_ms(x) sleep(x)

#include "../kernel/util.h"

void init_timer(uint32_t freq);
void sleep(int ms);
unsigned int GetTicks();
uint32_t timer_hz(void);
// lightweight periodic callbacks (1ms resolution)
int add_sub_timer(uint32_t duration_ms, void (*callback)(void));
void remove_sub_timer(int id);

// High-rate IRQ hook (called every PIT interrupt at current timer_hz)
void register_fast_tick(void (*cb)(void));
