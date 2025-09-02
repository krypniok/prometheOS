#include "perf.h"
#include "../cpu/timer.h"   // dein PIT sleep/ms
#include "../drivers/debug.h"   // optional debug_puts

static uint64_t g_cpu_hz = 0;

#include <stdint.h>

// Minimal 64/64 unsigned division helper to avoid libgcc recursion.
// Implements classic shift-subtract long division without using 64-bit division.
uint64_t __udivdi3(uint64_t n, uint64_t d) {
    if (d == 0) return 0xFFFFFFFFFFFFFFFFULL;
    uint64_t q = 0;
    uint64_t r = 0;
    for (int i = 63; i >= 0; --i) {
        r = (r << 1) | ((n >> i) & 1ULL);
        if (r >= d) { r -= d; q |= (1ULL << i); }
    }
    return q;
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

static int try_cpuid_tsc_freq(uint64_t *hz_out){
    uint32_t eax, ebx, ecx, edx;
    /* Leaf 0x15: TSC/Crystal clock info */
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x15), "c"(0));
    uint32_t denom = eax;      // EAX: denominator
    uint32_t numer = ebx;      // EBX: numerator
    uint32_t crystal = ecx;    // ECX: crystal clock Hz
    if (denom && numer && crystal) {
        uint64_t hz = ((uint64_t)crystal * (uint64_t)numer) / (uint64_t)denom;
        if (hz >= 100000000ULL && hz <= 10000000000ULL) { // 100 MHz .. 10 GHz sane range
            *hz_out = hz;
            return 1;
        }
    }
    /* Leaf 0x16: base frequency in MHz */
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x16));
    uint32_t base_mhz = eax;
    if (base_mhz) {
        uint64_t hz = (uint64_t)base_mhz * 1000000ULL;
        if (hz >= 100000000ULL && hz <= 10000000000ULL) {
            *hz_out = hz;
            return 1;
        }
    }
    return 0;
}

static uint64_t calibrate_tsc_via_pit(uint32_t ms)
{
    if (ms < 10) ms = 10; // ensure measurable
    // align to next tick
    uint32_t t = GetTicks();
    while (GetTicks() == t) { /* wait next 1ms tick */ }
    uint64_t start = perf_rdtsc();
    uint32_t t0 = GetTicks();
    while ((GetTicks() - t0) < ms) { /* spin */ }
    uint64_t end = perf_rdtsc();
    uint64_t delta = end - start;
    // scale to per-second
    // hz = delta * (1000/ms)
    uint64_t scale = 1000ULL;
    uint64_t hz = (delta * scale) / (uint64_t)ms;
    return hz;
}

void perf_init(void) {
    // In QEMU the CPUID base frequency (leaf 0x16) often reports ~1/10th
    // of the real TSC. Prefer PIT-based calibration for 1000 ms.
    g_cpu_hz = calibrate_tsc_via_pit(1000);

    // Avoid 64-bit printf on our tiny printf; print MHz as 32-bit
    uint32_t mhz = (uint32_t)(g_cpu_hz / 1000000ULL);
    printf("[perf] CPU freq ≈ %d MHz\n", mhz);
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

void sleep_us(uint64_t us){
    if (!g_cpu_hz) { perf_init(); }
    uint64_t ticks_per_us = g_cpu_hz / 1000000ULL;
    if (ticks_per_us == 0) ticks_per_us = 1; // avoid zero step
    uint64_t start = perf_rdtsc();
    uint64_t target = start + ticks_per_us * us;
    while (perf_rdtsc() < target) { /* spin */ }
}
