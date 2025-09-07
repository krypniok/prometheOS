#ifndef PERF_H
#define PERF_H

#include <stdint.h>

// perf_init: kalibriert TSC‑Ticks auf Hz
void perf_init(void);
// perf_rdtsc: rohen TSC auslesen
uint64_t perf_rdtsc(void);
// perf_cpu_hz: geschätzte CPU‑Frequenz in Hz
uint64_t perf_cpu_hz(void);
// micros/millis: monotone Zeit seit Boot (TSC‑basiert)
uint64_t micros(void);
uint64_t millis(void);
// sleep_us: aktive Wartezeit mittels TSC (präzise, blockierend)
void sleep_us(uint64_t us);

#endif
