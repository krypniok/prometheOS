#include "../../include/cpu/isr.h"
#include "../../include/cpu/idt.h"
#include "../../include/drivers/display.h"
#include "../../include/drivers/ports.h"
#include "../../include/util.h"

// --- Inline Assembly ISR Stubs ---
// Wir definieren die Stubs hier direkt, damit keine externe .asm Datei nötig ist.

__asm__(
    // Makro für ISR ohne Fehlercode
    ".macro ISR_NOERRCODE n\n"
    ".global isr\\n\n"
    "isr\\n:\n"
    "    cli\n"
    "    push $0\n"
    "    push $\\n\n"
    "    jmp isr_common_stub\n"
    ".endm\n"
    
    // Makro für ISR mit Fehlercode
    ".macro ISR_ERRCODE n\n"
    ".global isr\\n\n"
    "isr\\n:\n"
    "    cli\n"
    "    push $\\n\n"
    "    jmp isr_common_stub\n"
    ".endm\n"

    // Makro für IRQ
    ".macro IRQ n, m\n"
    ".global irq\\n\n"
    "irq\\n:\n"
    "    cli\n"
    "    push $0\n"
    "    push $\\m\n"
    "    jmp irq_common_stub\n"
    ".endm\n"

    // Gemeinsamer Stub für Exceptions
    "isr_common_stub:\n"
    "    pusha\n"
    "    mov %ds, %ax\n"
    "    push %eax\n"
    "    mov $0x10, %ax\n"
    "    mov %ds, %ax\n"
    "    mov %es, %ax\n"
    "    mov %fs, %ax\n"
    "    mov %gs, %ax\n"
    "    push %esp\n"
    "    call isr_handler\n"
    "    add $4, %esp\n"
    "    pop %eax\n"
    "    mov %ax, %ds\n"
    "    mov %ax, %es\n"
    "    mov %ax, %fs\n"
    "    mov %ax, %gs\n"
    "    popa\n"
    "    add $8, %esp\n"
    "    sti\n"
    "    iret\n"

    // Gemeinsamer Stub für IRQs
    "irq_common_stub:\n"
    "    pusha\n"
    "    mov %ds, %ax\n"
    "    push %eax\n"
    "    mov $0x10, %ax\n"
    "    mov %ds, %ax\n"
    "    mov %es, %ax\n"
    "    mov %fs, %ax\n"
    "    mov %gs, %ax\n"
    "    push %esp\n"
    "    call irq_handler\n"
    "    add $4, %esp\n"
    "    pop %eax\n"
    "    mov %ax, %ds\n"
    "    mov %ax, %es\n"
    "    mov %ax, %fs\n"
    "    mov %ax, %gs\n"
    "    popa\n"
    "    add $8, %esp\n"
    "    sti\n"
    "    iret\n"

    // Instanziierung der Handler
    "ISR_NOERRCODE 0\n"
    "ISR_NOERRCODE 1\n"
    "ISR_NOERRCODE 2\n"
    "ISR_NOERRCODE 3\n"
    "ISR_NOERRCODE 4\n"
    "ISR_NOERRCODE 5\n"
    "ISR_NOERRCODE 6\n"
    "ISR_NOERRCODE 7\n"
    "ISR_ERRCODE   8\n"
    "ISR_NOERRCODE 9\n"
    "ISR_ERRCODE   10\n"
    "ISR_ERRCODE   11\n"
    "ISR_ERRCODE   12\n"
    "ISR_ERRCODE   13\n"
    "ISR_ERRCODE   14\n"
    "ISR_NOERRCODE 15\n"
    "ISR_NOERRCODE 16\n"
    "ISR_ERRCODE   17\n"
    "ISR_NOERRCODE 18\n"
    "ISR_NOERRCODE 19\n"
    "ISR_NOERRCODE 20\n"
    "ISR_NOERRCODE 21\n"
    "ISR_NOERRCODE 22\n"
    "ISR_NOERRCODE 23\n"
    "ISR_NOERRCODE 24\n"
    "ISR_NOERRCODE 25\n"
    "ISR_NOERRCODE 26\n"
    "ISR_NOERRCODE 27\n"
    "ISR_NOERRCODE 28\n"
    "ISR_NOERRCODE 29\n"
    "ISR_NOERRCODE 30\n"
    "ISR_NOERRCODE 31\n"

    "IRQ   0,    32\n"
    "IRQ   1,    33\n"
    "IRQ   2,    34\n"
    "IRQ   3,    35\n"
    "IRQ   4,    36\n"
    "IRQ   5,    37\n"
    "IRQ   6,    38\n"
    "IRQ   7,    39\n"
    "IRQ   8,    40\n"
    "IRQ   9,    41\n"
    "IRQ  10,    42\n"
    "IRQ  11,    43\n"
    "IRQ  12,    44\n"
    "IRQ  13,    45\n"
    "IRQ  14,    46\n"
    "IRQ  15,    47\n"
);

isr_t interrupt_handlers[256];

/* Can't do this with a loop because we need the address
 * of the function names */
void isr_install() {
    set_idt_gate(0, (uint32_t) isr0);
    set_idt_gate(1, (uint32_t) isr1);
    set_idt_gate(2, (uint32_t) isr2);
    set_idt_gate(3, (uint32_t) isr3);
    set_idt_gate(4, (uint32_t) isr4);
    set_idt_gate(5, (uint32_t) isr5);
    set_idt_gate(6, (uint32_t) isr6);
    set_idt_gate(7, (uint32_t) isr7);
    set_idt_gate(8, (uint32_t) isr8);
    set_idt_gate(9, (uint32_t) isr9);
    set_idt_gate(10, (uint32_t) isr10);
    set_idt_gate(11, (uint32_t) isr11);
    set_idt_gate(12, (uint32_t) isr12);
    set_idt_gate(13, (uint32_t) isr13);
    set_idt_gate(14, (uint32_t) isr14);
    set_idt_gate(15, (uint32_t) isr15);
    set_idt_gate(16, (uint32_t) isr16);
    set_idt_gate(17, (uint32_t) isr17);
    set_idt_gate(18, (uint32_t) isr18);
    set_idt_gate(19, (uint32_t) isr19);
    set_idt_gate(20, (uint32_t) isr20);
    set_idt_gate(21, (uint32_t) isr21);
    set_idt_gate(22, (uint32_t) isr22);
    set_idt_gate(23, (uint32_t) isr23);
    set_idt_gate(24, (uint32_t) isr24);
    set_idt_gate(25, (uint32_t) isr25);
    set_idt_gate(26, (uint32_t) isr26);
    set_idt_gate(27, (uint32_t) isr27);
    set_idt_gate(28, (uint32_t) isr28);
    set_idt_gate(29, (uint32_t) isr29);
    set_idt_gate(30, (uint32_t) isr30);
    set_idt_gate(31, (uint32_t) isr31);

    // Remap the PIC
    port_byte_out(0x20, 0x11);
    port_byte_out(0xA0, 0x11);
    port_byte_out(0x21, 0x20);
    port_byte_out(0xA1, 0x28);
    port_byte_out(0x21, 0x04);
    port_byte_out(0xA1, 0x02);
    port_byte_out(0x21, 0x01);
    port_byte_out(0xA1, 0x01);
    port_byte_out(0x21, 0x0);
    port_byte_out(0xA1, 0x0);

    // Install the IRQs
    set_idt_gate(32, (uint32_t)irq0);
    set_idt_gate(33, (uint32_t)irq1);
    set_idt_gate(34, (uint32_t)irq2);
    set_idt_gate(35, (uint32_t)irq3);
    set_idt_gate(36, (uint32_t)irq4);
    set_idt_gate(37, (uint32_t)irq5);
    set_idt_gate(38, (uint32_t)irq6);
    set_idt_gate(39, (uint32_t)irq7);
    set_idt_gate(40, (uint32_t)irq8);
    set_idt_gate(41, (uint32_t)irq9);
    set_idt_gate(42, (uint32_t)irq10);
    set_idt_gate(43, (uint32_t)irq11);
    set_idt_gate(44, (uint32_t)irq12);
    set_idt_gate(45, (uint32_t)irq13);
    set_idt_gate(46, (uint32_t)irq14);
    set_idt_gate(47, (uint32_t)irq15);

    load_idt(); // Load with ASM
}

/* To print the message which defines every exception */
char *exception_messages[] = {
        "Division By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Into Detected Overflow",
        "Out of Bounds",
        "Invalid Opcode",
        "No Coprocessor",

        "Double Fault",
        "Coprocessor Segment Overrun",
        "Bad TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault",
        "Unknown Interrupt",

        "Coprocessor Fault",
        "Alignment Check",
        "Machine Check",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",

        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved"
};

void isr_handler(registers_t *r) {
    print_string("received interrupt: ");
    char s[3];
    intToString(r->int_no, s);
    print_string(s);
    print_nl();
    print_string(exception_messages[r->int_no]);
    print_nl();

    isr_t handler = interrupt_handlers[IRQ10];
    handler(r);
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

void irq_handler(registers_t *r) {
    /* Handle the interrupt in a more modular way */
    if (interrupt_handlers[r->int_no] != 0) {
        isr_t handler = interrupt_handlers[r->int_no];
        handler(r);
    }

    // EOI
    if (r->int_no >= 40) {
        port_byte_out(0xA0, 0x20); /* follower */
    }
    port_byte_out(0x20, 0x20); /* leader */
}
