#pragma once

#include <stdint.h>

// Initialize time base from CMOS RTC and perf micros
void time_init_with_rtc(void);

// Current epoch seconds (since 1970-01-01 UTC)
uint64_t time_now_seconds(void);

// Format current time into ISO8601-like string (YYYY-MM-DD HH:MM:SS)
void time_now_iso(char* buf, int bufsz);

