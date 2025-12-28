
// Central time API (umbrella header)
#pragma once

#include <stdint.h>

// Coarse sleep via PIT (milliseconds)
void sleep(int ms);
// Fine-grained busy-wait based on TSC (microseconds)
void sleep_us(uint64_t us);

// Monotonic time since boot based on TSC calibration
uint64_t micros(void);
uint64_t millis(void);

// PIT tick counter (1 kHz)
unsigned int GetTicks(void);

// Formatting helpers for durations
extern unsigned char g_strUptime[80];
void fmt_timespan(unsigned int timestamp_ms, unsigned char* out);
