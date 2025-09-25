#include "timer.h"
#include "../drivers/display.h"
#include "../drivers/ports.h"
#include "../kernel/util.h"
#include "isr.h"
#include "../kernel/thread.h"
// Provide weak hook so builds without threading object still link
void __attribute__((weak)) thread_preempt_tick(registers_t* r) { (void)r; }
#include "../kernel/perf.h"


#define MAX_SUB_TIMERS 256

typedef struct {
    uint32_t remaining_time;
    uint32_t duration;
    void (*callback)(void);
} SubTimer;

SubTimer sub_timers[MAX_SUB_TIMERS];
uint8_t num_sub_timers = 0;

// Millisecond tick counter for regular timing APIs
static uint32_t g_ms_tick = 0;
// Configured PIT frequency (Hz)
static uint32_t g_timer_hz = 1000;
// Accumulator to convert high-rate PIT ticks into whole milliseconds
static uint32_t g_ms_accum = 0; // counts in units of "ms*Hz" (add 1000 per IRQ)
// Optional high-frequency callback (called each IRQ at g_timer_hz)
static void (*g_fast_tick_cb)(void) = 0;

void update_sub_timers(uint32_t elapsed_time);

// Expose timer hz for other modules
uint32_t timer_hz(void) { return g_timer_hz; }
unsigned int GetTicks() { return g_ms_tick; }
void register_fast_tick(void (*cb)(void)) { g_fast_tick_cb = cb; }

static void timer_callback(registers_t *regs) {
    // High-rate hook (e.g., audio at 11.025 kHz)
    if (g_fast_tick_cb) g_fast_tick_cb();

    // Accumulate milliseconds from configured PIT frequency
    // Add 1000 (ms per second) each IRQ; once we reach g_timer_hz, one ms elapsed
    g_ms_accum += 1000;
    if (g_ms_accum >= g_timer_hz) {
        uint32_t ms = g_ms_accum / g_timer_hz;
        g_ms_accum -= ms * g_timer_hz;
        g_ms_tick += ms;
        update_sub_timers(ms);
    }

    // Preemption bookkeeping per IRQ
    thread_preempt_tick(regs);
}

// Exposed for IRQ stub to switch stacks on preemptive scheduling
volatile unsigned int g_sched_new_esp = 0;

void sleep(int ms) {
    if (ms <= 0) return;
    // If TSC is calibrated, measure against micros() to ensure ms granularity
    if (perf_cpu_hz() != 0) {
        uint64_t target = micros() + (uint64_t)ms * 1000ULL;
        while (micros() < target) { /* spin */ }
        return;
    }
    // Fallback to PIT ticks at ~1 kHz
    uint32_t ts = g_ms_tick;
    while ((g_ms_tick - ts) < (uint32_t)ms) { /* spin */ }
}

void sub_timer_callback() {
    static int ff=0;
    ff = (ff == 0) ? 1 : 0;
 //   printChar(79, 24, 0x0F, (ff == 0) ? 0x01 : 0x02);
 //   int cursor = get_cursor();
 //   set_cursor(144);
 //   formatTimestampHHMMSS(tick);
 //   set_cursor(cursor);

}

void sub_timer_cursor_callback() {
    static int ff=0;
    if( isCursorVisible() == 0 ) return;
    ff = (ff == 0) ? 1 : 0;
    if(ff) { 
        int cursor = get_cursor();
        printf("%c", get_cursor_char());
        set_cursor(cursor);
    } else {
        int cursor = get_cursor();
        printf("%c", 0x20);
        set_cursor(cursor);
    }
}

// init_custom_timer
void init_timer(uint32_t freq) {
    /* Install the function we just wrote */
    register_interrupt_handler(IRQ0, timer_callback);

    /* Get the PIT value: hardware clock at 1193180 Hz */
    g_timer_hz = (freq == 0) ? 1000 : freq;
    uint32_t divisor = 1193180 / freq;
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    /* Send the command */
    port_byte_out(0x43, 0x36); /* Command port */
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);

    for(int i=0; i<MAX_SUB_TIMERS; i++) {
        sub_timers[i].callback = NULL;
        sub_timers[i].remaining_time = 0;
        sub_timers[i].duration = 0;
    }

    add_sub_timer(1, sub_timer_callback);
    //hidecursor();
    //add_sub_timer(500, sub_timer_cursor_callback);
}

int add_sub_timer(uint32_t duration, void (*callback)(void)) {
    if (num_sub_timers >= MAX_SUB_TIMERS) {
        // Max number of sub-timers reached, handle accordingly
        return -1;
    }

    for(int i=0; i<MAX_SUB_TIMERS; i++) {
        if(sub_timers[i].callback == NULL) {
            num_sub_timers++;
            sub_timers[i].remaining_time = duration;
            sub_timers[i].duration = duration;
            sub_timers[i].callback = callback;
            return i;
        }
    }

    return -1;
}

void remove_sub_timer(int id) {
    sub_timers[id].callback = NULL;
    sub_timers[id].remaining_time = 0;
    sub_timers[id].duration = 0;
    num_sub_timers--;
}

void update_sub_timers(uint32_t elapsed_time) {
    // Iterate the full table to avoid holes skipping timers
    for (uint16_t i = 0; i < MAX_SUB_TIMERS; i++) {
        if (sub_timers[i].callback == NULL) continue;
        if (sub_timers[i].remaining_time > elapsed_time) {
            sub_timers[i].remaining_time -= elapsed_time;
        } else {
            // fire
            sub_timers[i].callback();
            // Reset the sub-timer to its original duration
            sub_timers[i].remaining_time = sub_timers[i].duration;
        }
    }
}
 
