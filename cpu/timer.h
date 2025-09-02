#pragma once

#define delay_ms(x) sleep(x)

#include "../kernel/util.h"

void init_timer(uint32_t freq);
void sleep(int ms);
unsigned int GetTicks();
// lightweight periodic callbacks (1ms resolution)
int add_sub_timer(uint32_t duration_ms, void (*callback)(void));
void remove_sub_timer(int id);
