#pragma once

#include "cpu/jmpbuf.h"
#include "cpu/isr.h"

#define MAX_THREADS 32

typedef struct {
    int id;
    int active;
    int should_stop;
    void *entry;
    void *stack;
} thread_info_t;

void thread_init(void);
int thread_create(void (*fn)(void));
void thread_exit(void);
void thread_yield(void);
int thread_should_stop(void);
int thread_kill(int tid);
int thread_current_id(void);
int thread_enumerate(thread_info_t *infos, int max);

// Wird vom Timer-Interrupt aufgerufen
void thread_preempt_tick(registers_t* regs);