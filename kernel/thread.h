/* thread.h */
#ifndef THREAD_H
#define THREAD_H

#include "../cpu/jmpbuf.h"
#include "../cpu/isr.h"

#define MAX_THREADS  16
#define STACK_SIZE   4096

typedef struct thread {
    jmp_buf ctx;               // cooperative context
    unsigned char *stack;      // kernel stack base
    int active;                // runnable?
    void (*entry)(void);       // entry function (preemptive)
    registers_t* frame;        // saved IRQ frame (preemptive)
    volatile int should_stop;  // cooperative kill flag
} thread_t;

void thread_init(void);
int  thread_create(void (*fn)(void));
void thread_yield(void);
void thread_exit(void);
int  thread_join(int thread_id);
int  thread_kill(int thread_id);
int  thread_should_stop(void);
int  thread_current_id(void);

// Simple demo/test command: spawns two threads and schedules them
void thread_test(void);

// Scheduler control/inspection
void sched_info(void);
void sched_timeslice(unsigned int ms);
void sched_mode(const char* mode); // "preempt" or "coop"

// Preemptive scheduler hook (IRQ0)
void thread_preempt_tick(registers_t* r);

#endif
