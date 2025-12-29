#include "../include/perf.h"
#include "../include/drivers/timer.h"

void perf_init(void) {
    // TODO: Kalibrierung via PIT/RTC
}

uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

uint64_t perf_cpu_hz(void) {
    return 0; // Noch nicht implementiert -> Fallback auf PIT
}

uint64_t micros(void) {
    return (uint64_t)timer_ticks * 1000ULL;
}