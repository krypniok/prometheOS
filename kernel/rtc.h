#pragma once

#include <stdint.h>

// RTC initialisieren: Baseline von CMOS‑RTC + perf‑Micros
void time_init_with_rtc(void);

// Epoch‑Sekunden (seit 1970‑01‑01 UTC)
uint64_t time_now_seconds(void);

// Zeit formatieren: YYYY-MM-DD HH:MM:SS
void time_now_iso(char* buf, int bufsz);
