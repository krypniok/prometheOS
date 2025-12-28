#include "../include/io.h"
#include "../include/drivers/keyboard.h"
#include "../include/drivers/mouse.h"
#include "../include/drivers/sb16.h"
#include "../include/interrupts.h"

// Externer Scheduler (noch in kernel.c)
extern unsigned int scheduler(unsigned int current_esp);

// --- IDT & Interrupts ---
struct idt_entry_struct {
    unsigned short base_low;
    unsigned short sel;
    unsigned char  always0;
    unsigned char  flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr_struct {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct   idt_ptr;

void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel     = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags   = flags;
}

// ISR for Timer (IRQ0) - Handles Context Switch
__attribute__((naked)) void irq0_isr() {
    __asm__ volatile (
        "pusha\n\t"             // Save registers
        "push %esp\n\t"         // Pass current ESP to scheduler
        "call scheduler\n\t"    // Call scheduler, returns new ESP
        "mov %eax, %esp\n\t"    // Switch stack
        "popa\n\t"              // Restore registers (of new task)
        "iret\n\t"
    );
}

__attribute__((naked)) void irq1_isr() {
    __asm__ volatile ("pusha; call keyboard_handler; popa; iret");
}

__attribute__((naked)) void irq12_isr() {
    __asm__ volatile ("pusha; call mouse_handler; popa; iret");
}

__attribute__((naked)) void irq5_isr() {
    __asm__ volatile ("pusha; call sb16_irq_handler; popa; iret");
}

void init_interrupts() {
    idt_ptr.limit = sizeof(struct idt_entry_struct) * 256 - 1;
    idt_ptr.base  = (unsigned int)&idt_entries;

    // PIC Remapping (Master 0x20, Slave 0x28)
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    
    // Maskierung:
    // Master: IRQ 0 (Timer), 1 (Kbd), 2 (Cascade für Slave) an -> 1111 1000 = 0xF8
    // Slave: IRQ 12 (Maus) an -> 1110 1111 = 0xEF
    // Neu: IRQ 5 (SB16) auch an. Bit 5 = 0. 0xF8 & ~(1<<5) = 0xD8
    outb(0x21, 0xD8); outb(0xA1, 0xEF);

    // IRQ0->32, IRQ1->33, IRQ12->44
    idt_set_gate(32, (unsigned int)irq0_isr, 0x08, 0x8E);
    idt_set_gate(33, (unsigned int)irq1_isr, 0x08, 0x8E);
    idt_set_gate(37, (unsigned int)irq5_isr, 0x08, 0x8E);
    idt_set_gate(44, (unsigned int)irq12_isr, 0x08, 0x8E);

    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
    __asm__ volatile ("sti"); // Interrupts aktivieren!
}