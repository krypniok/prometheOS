#ifndef PERF_H
#define PERF_H

#include <stdint.h>

void perf_init(void);          // kalibriert den TSC
uint64_t perf_rdtsc(void);     // roher TSC auslesen
uint64_t perf_cpu_hz(void);    // gibt ermittelte MHz/Hz zurück
uint64_t micros(void);         // aktuelle µs seit boot
uint64_t millis(void);         // aktuelle ms seit boot

#endif
