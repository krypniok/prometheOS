#include "../drivers/ports.h"
#include "../kernel/util.h"
#include "../cpu/isr.h"

static void video_callback(registers_t *regs) {
   print_string("Video Interrupt: ");
}

void init_video() {
    /* Install the function we just wrote */
    register_interrupt_handler(IRQ10, video_callback);
}
