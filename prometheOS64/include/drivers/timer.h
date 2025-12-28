#ifndef TIMER_H
#define TIMER_H

extern volatile unsigned int timer_ticks;

void init_timer(unsigned int freq);
void sleep(unsigned int ticks);

#endif