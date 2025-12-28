#include "../include/io.h"
#include "../include/drivers/timer.h"

volatile unsigned int timer_ticks = 0;

// PIT initialisieren (1000 Hz für ms-Genauigkeit)
void init_timer(unsigned int freq) {
    unsigned int divisor = 1193180 / freq;
    outb(0x43, 0x36); // Channel 0, lo/hi, square wave, binary
    io_wait();
    outb(0x40, divisor & 0xFF);
    io_wait();
    outb(0x40, (divisor >> 8) & 0xFF);
}

void sleep(unsigned int ticks) {
    unsigned int eticks = timer_ticks + ticks;
    while(timer_ticks < eticks) {
        __asm__ volatile ("hlt"); // CPU schlafen legen bis zum nächsten Interrupt
    }
}