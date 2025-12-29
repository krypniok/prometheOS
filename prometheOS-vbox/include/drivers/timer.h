#ifndef TIMER_H
#define TIMER_H

extern volatile unsigned int timer_ticks;

void init_timer(unsigned int freq);
void sleep(unsigned int ticks);

// Busy-wait for microseconds (approximate)
static inline void sleep_us(unsigned int us) {
    // Simple loop for now, assuming ~1GHz CPU and some overhead
    for (volatile unsigned int i = 0; i < us * 100; i++) { __asm__ volatile("nop"); }
}

#endif