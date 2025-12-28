#ifndef SCHEDULER_H
#define SCHEDULER_H

void init_multitasking();
void task_create(void (*func)());
unsigned int scheduler(unsigned int current_esp);

#endif