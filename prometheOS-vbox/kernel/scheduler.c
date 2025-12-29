#include "../include/scheduler.h"
#include "../include/io.h"
#include "../include/drivers/timer.h"
#include "../include/drivers/console.h"

#define MAX_TASKS 4
#define STACK_SIZE 4096

typedef struct {
    unsigned int esp;
    unsigned int active;
} task_t;

static task_t tasks[MAX_TASKS];
static unsigned char task_stacks[MAX_TASKS][STACK_SIZE];
static int current_task = 0;
static int num_tasks = 0;

void init_multitasking() {
    // Task 0 ist der aktuelle Kernel-Thread (Shell)
    tasks[0].active = 1;
    num_tasks = 1;
    current_task = 0;
    term_puts("[KERNEL] Multitasking initialized\n");
}

void task_create(void (*func)()) {
    if (num_tasks >= MAX_TASKS) return;
    int id = num_tasks++;
    tasks[id].active = 1;
    
    // Setup stack for the new task (grows downwards)
    unsigned int *stack = (unsigned int*)(task_stacks[id] + STACK_SIZE);
    
    // Initial stack layout for iret (Interrupt Return)
    *(--stack) = 0x202; // EFLAGS (Interrupts enabled)
    *(--stack) = 0x08;  // CS (Kernel Code Segment)
    *(--stack) = (unsigned int)func; // EIP
    
    // PUSHA frame (EAX..EDI) - initialisiert mit 0
    *(--stack) = 0; *(--stack) = 0; *(--stack) = 0; *(--stack) = 0;
    *(--stack) = 0; *(--stack) = 0; *(--stack) = 0; *(--stack) = 0;
    
    tasks[id].esp = (unsigned int)stack;
}

unsigned int scheduler(unsigned int current_esp) {
    // Save current task's ESP
    if (num_tasks > 0) {
        tasks[current_task].esp = current_esp;
    }

    // Timer Logic (wird hier erledigt, da IRQ0 direkt hierher springt)
    timer_ticks++;
    outb(0x20, 0x20); // EOI an Master PIC senden

    // Round Robin Switching
    if (num_tasks > 1) {
        current_task++;
        if (current_task >= num_tasks) current_task = 0;
    }
    
    return tasks[current_task].esp;
}