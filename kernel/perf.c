#include "perf.h"
#include "../cpu/timer.h"   // dein PIT sleep/ms
#include "../drivers/debug.h"   // optional debug_puts

static uint64_t g_cpu_hz = 0;

#include <stdint.h>

uint64_t __udivdi3(uint64_t n, uint64_t d) {
    return n / d;
}

uint64_t perf_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void perf_init2(void) {
    // wir messen 50ms
    uint64_t start = perf_rdtsc();
    sleep(50); // dein bestehendes PIT-Sleep
    uint64_t end = perf_rdtsc();

    g_cpu_hz = (end - start) * 20; // auf 1s hochrechnen

    char buf[64];
    sprintf(buf,"[perf] CPU freq ≈ %i Hz\n", g_cpu_hz);
    debug_puts(buf);
}

void perf_init(void) {
    uint64_t start = perf_rdtsc();

    // PIT ticks abwarten: 50 ms = 50 Ticks bei init_timer(1000)
    uint32_t t0 = GetTicks();
    while (GetTicks() - t0 < 50);

    uint64_t end = perf_rdtsc();
    g_cpu_hz = (end - start) * 20;  // *20 weil 50ms → hoch auf 1s
    char buf[64];
    sprintf(buf,"[perf] CPU freq ≈ %i Hz\n", g_cpu_hz);
    debug_puts(buf);

}


uint64_t perf_cpu_hz(void) {
    return g_cpu_hz;
}

uint64_t micros(void) {
    return (perf_rdtsc() * 1000000ULL) / g_cpu_hz;
}

uint64_t millis(void) {
    return (perf_rdtsc() * 1000ULL) / g_cpu_hz;
}
