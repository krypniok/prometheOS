#pragma once

#define delay_ms(x) sleep(x)

#include "../kernel/util.h"

void init_timer(uint32_t freq);
void sleep(int ms);
unsigned int GetTicks();
