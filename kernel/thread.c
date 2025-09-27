/* thread.c */
#include <stdbool.h>

#include "thread.h"
#include "../drivers/display.h"
#include "../stdlibs/memory.h"
#include "../stdlibs/string.h"
#include "../kernel/perf.h"

void kernel_request_shutdown(void) __attribute__((weak));
void kernel_request_shutdown(void) { }

// Symbol used by IRQ stub to switch stacks on preemptive scheduling
extern volatile unsigned int g_sched_new_esp;

static void thread_trampoline(void);

static thread_t threads[MAX_THREADS];
static int current = 0;
static int nthreads = 0;
static int g_sched_mode = 0; // default cooperative (no IRQ preemption)

extern void switch_to(thread_t *t); /* asm helper evtl. */

void thread_init(void) {
    for (int i=0;i<MAX_THREADS;i++) threads[i].active=0;
    nthreads=1;
    threads[0].active=1; // main
    threads[0].entry = 0;
    threads[0].frame = 0;
    threads[0].should_stop = 0;
}

int thread_create(void (*fn)(void)) {
    // Lazy init if not yet initialized
    if (nthreads == 0) thread_init();
    if(nthreads>=MAX_THREADS) return -1;
    int id=nthreads++;
    threads[id].stack=(unsigned char*)malloc(STACK_SIZE);
    if(!threads[id].stack) return -1;
    if (g_sched_mode) {
        // preemptive: build initial IRQ frame to enter thread_trampoline via iret
        threads[id].entry = fn;
        registers_t* fr = (registers_t*)(threads[id].stack + STACK_SIZE - sizeof(registers_t));
        memset(fr, 0, sizeof(registers_t));
        fr->ds = 0x10;        // kernel data selector
        fr->cs = 0x08;        // kernel code selector
        fr->eflags = 0x202;   // IF=1
        fr->eip = (unsigned int)thread_trampoline;
        threads[id].frame = fr;
        threads[id].active = 1;
        threads[id].should_stop = 0;
    } else {
        // cooperative: setjmp to init, then enter via trampoline
        setjmp(&threads[id].ctx);
        threads[id].entry = fn;
        threads[id].ctx.eip=(unsigned int)thread_trampoline;
        threads[id].ctx.esp=(unsigned int)(threads[id].stack+STACK_SIZE-4);
        threads[id].active=1;
        threads[id].should_stop = 0;
    }
    return id;
}

void thread_yield(void) {
    int old=current;
    if(setjmp(&threads[old].ctx)==0) {
        do {
            current=(current+1)%nthreads;
        } while(!threads[current].active);
        longjmp(&threads[current].ctx,1);
    }
}

void thread_exit(void) {
    threads[current].active=0;
    if (g_sched_mode) {
        // preemptive: spin until timer preempts us (no HLT outside end_of_kernel)
        for(;;){ asm volatile("nop"); }
    } else {
        thread_yield();
    }
}

static void thread_trampoline(void){
    void (*fn)(void) = threads[current].entry;
    if (fn) fn();
    thread_exit();
}

// --- Preemptive scheduler (IRQ0 hook) + CLI ---------------------------------
static int g_slice_ticks = 10; // ms at 1kHz
static int g_ticks_since_switch = 0;

void sched_info(void){
    int runnable = 0, zombies = 0;
    for (int i=0;i<nthreads;i++) {
        if (threads[i].active) runnable++; else zombies++;
    }
    if (g_sched_mode)
        printf("scheduler: preemptive, slice=%d ms, total=%d runnable=%d zombies=%d current=%d\n",
               g_slice_ticks, nthreads, runnable, zombies, current);
    else
        printf("scheduler: cooperative, total=%d runnable=%d zombies=%d current=%d\n",
               nthreads, runnable, zombies, current);
}

void sched_timeslice(unsigned int ms){ if (ms<1) ms=1; if (ms>1000) ms=1000; g_slice_ticks=(int)ms; printf("timeslice set to %d ms\n", g_slice_ticks); }
void sched_mode(const char* mode){
    if (!mode) { printf("usage: sched_mode <preempt|coop>\n"); return; }
    if (strcmp(mode,"preempt")==0){ g_sched_mode=1; printf("scheduler mode: preemptive\n"); }
    else if (strcmp(mode,"coop")==0){ g_sched_mode=0; printf("scheduler mode: cooperative\n"); }
    else { printf("unknown mode: %s\n", mode); }
}

void thread_preempt_tick(registers_t* r){
    if (!g_sched_mode) return;
    // Save current frame always
    threads[current].frame = r;

    // Count active threads
    int active_count = 0;
    for (int i=0;i<nthreads;i++) if (threads[i].active) active_count++;

    // If current finished, switch immediately
    if (!threads[current].active) {
        int next = current, found = 0;
        for (int i=0;i<nthreads;i++){ next = (next+1)%nthreads; if (threads[next].active){ found=1; break; } }
        if (found){ current = next; if (threads[current].frame) g_sched_new_esp = (unsigned int)threads[current].frame; }
        return;
    }

    if (active_count <= 1) return; // nothing to rotate

    if (++g_ticks_since_switch < g_slice_ticks) return;
    g_ticks_since_switch = 0;

    int next = current, found = 0;
    for (int i=0;i<nthreads;i++){ next = (next+1)%nthreads; if (threads[next].active){ found=1; break; } }
    if (!found || next == current) return;
    current = next;
    if (threads[current].frame) g_sched_new_esp = (unsigned int)threads[current].frame;
}

// --- Simple cooperative threading demo ------------------------------------
static void worker_a(void){
    for (int i=1;i<=5;i++){
        printf("[A%d] ", i);
    }
    printf("[A done]\n");
}

static void worker_b(void){
    for (int i=1;i<=5;i++){
        printf("[B%d] ", i);
    }
    printf("[B done]\n");
}

void thread_test(void){
    printf("thread_test: spawn two workers\n");
    int ta = thread_create(worker_a);
    int tb = thread_create(worker_b);
    if (ta < 0 || tb < 0) { printf("thread_test: create failed\n"); return; }
    // Join in both modes; in cooperative mode, yield so workers can run
    thread_join(ta);
    thread_join(tb);
    printf("\nthread_test: done\n");
}

int thread_join(int thread_id){
    if (thread_id <= 0 || thread_id >= nthreads) return -1;
    while (threads[thread_id].active) {
        if (!g_sched_mode) { thread_yield(); }
        else { sleep_us(1000); }
    }
    return 0;
}

int thread_kill(int thread_id){
    if (thread_id <= 0 || thread_id >= nthreads) return -1;
    threads[thread_id].should_stop = 1;
    if (thread_id == 1) {
        kernel_request_shutdown();
    }
    return 0;
}

int thread_should_stop(void){ return threads[current].should_stop ? 1 : 0; }

int thread_current_id(void){ return current; }

int thread_enumerate(thread_info_t* out, int max_entries) {
    if (!out || max_entries <= 0) return 0;
    int count = 0;
    for (int i = 0; i < nthreads && count < max_entries; ++i) {
        out[count].id = i;
        out[count].active = threads[i].active;
        out[count].should_stop = threads[i].should_stop;
        out[count].entry = threads[i].entry;
        out[count].frame = threads[i].frame;
        out[count].stack = threads[i].stack;
        count++;
    }
    return count;
}

int thread_count(void) { return nthreads; }
