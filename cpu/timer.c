#include "timer.h"
#include "../drivers/display.h"
#include "../drivers/ports.h"
#include "../kernel/util.h"
#include "isr.h"


#define MAX_SUB_TIMERS 256

typedef struct {
    uint32_t remaining_time;
    uint32_t duration;
    void (*callback)(void);
} SubTimer;

SubTimer sub_timers[MAX_SUB_TIMERS];
uint8_t num_sub_timers = 0;

uint32_t tick = 0;

void update_sub_timers(uint32_t elapsed_time);

static void timer_callback(registers_t *regs) {
    tick++;
    update_sub_timers(1); // Call this with a suitable interval (e.g., 1 ms) to update sub-timers
}

void sleep(int ms) {
    uint32_t ts = tick;
    setCursorVisible(0);
    while ((tick - ts) < ms) { }
    setCursorVisible(1);
}

unsigned int GetTicks() {
    return tick;
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
    uint32_t divisor = 1193180 / freq;
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    /* Send the command */
    port_byte_out(0x43, 0x36); /* Command port */
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);

    for(int i=0; i<MAX_SUB_TIMERS; i++) {
        sub_timers[i].callback == NULL;
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
    num_sub_timers--;
}

void update_sub_timers(uint32_t elapsed_time) {
    for (uint8_t i = 0; i < num_sub_timers; i++) {
        if (sub_timers[i].remaining_time > elapsed_time) {
            sub_timers[i].remaining_time -= elapsed_time;
        } else {
            if (sub_timers[i].callback != NULL) {
                sub_timers[i].callback();
            }

            // Reset the sub-timer to its original duration
            sub_timers[i].remaining_time = sub_timers[i].duration;
        }
    }
}

