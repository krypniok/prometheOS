#pragma once
#include <stdint.h>

void perf_init(void);
uint64_t perf_cpu_hz(void);
uint64_t rdtsc(void);
uint64_t micros(void);