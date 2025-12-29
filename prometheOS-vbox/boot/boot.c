/* boot.c - MBR in C mit Inline-Assembly */

// Wir sagen dem Compiler, dass dies 16-Bit Code ist (für den Real Mode)
__asm__(".code16gcc"); 

#define COM1 0x3F8

// Port I/O primitive
static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm__ volatile ("inb %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %w1" : : "a"(val), "Nd"(port));
}

// COM1: schreibe ein Byte (wartet auf THR empty)
static inline void com1_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20));
    outb(COM1, (unsigned char)c);
}

// BIOS-Teletype (INT 0x10 AH=0x0E) - nicht im Boot nutzen (zu groß)
static inline void bios_putc(char c) {
    __asm__ volatile (
        "movb $0x0E, %%ah\n\t"
        "int $0x10\n\t"
        : : "a"(c) : "bx"
    );
}

// putc: Immer auf beiden Kanälen ausgeben (MBR Anforderung)
static inline void putc(char c) {
    com1_putc(c);
    bios_putc(c);
}

// puts
static void puts(const char *s) {
    for (const char *p = s; *p; ++p) putc(*p);
}

// Hex-Ausgabe Helper (8-Bit)
static void print_hex8(unsigned char val) {
    const char *hex = "0123456789ABCDEF";
    putc(hex[(val >> 4) & 0xF]);
    putc(hex[val & 0xF]);
}

// Der Einsprungpunkt muss ganz am Anfang stehen
__attribute__((section(".boot"))) __attribute__((naked))
void boot_main() {
    __asm__ volatile (
        "cli\n\t"                // Interrupts aus
        "ljmp $0, $1f\n\t"       // CS auf 0 normalisieren (VBox Fix!)
        "1:\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%ds\n\t"     // DS = 0
        "mov %%ax, %%es\n\t"     // ES = 0
        "mov %%ax, %%ss\n\t"     // SS = 0
        "mov $0x7C00, %%sp\n\t"  // Stack unter den Bootloader setzen
        "movb %%dl, 0x7B00\n\t"  // DL sichern an fester Adresse (0x7B00)
        
        // COM1 Initialisierung (UART)
        "mov $0x3FB, %%dx\n\t"   // Line Control Register
        "mov $0x80, %%al\n\t"    // DLAB Bit setzen
        "out %%al, %%dx\n\t"
        "mov $0x3F8, %%dx\n\t"   // Divisor Latch Low
        "mov $0x01, %%al\n\t"    // 115200 Baud (bei 1.8MHz)
        "out %%al, %%dx\n\t"
        "mov $0x3FB, %%dx\n\t"   // DLAB aus, 8N1
        "mov $0x03, %%al\n\t"    // 8N1
        "out %%al, %%dx\n\t"
        : : : "ax", "dx"
    );

    // Kurze Startmeldung (nur COM1)
    puts("[MBR]\r\n");
    
    // A20 aktivieren (notwendig für Adressierung nach 0x10000)
    __asm__ volatile (
        // Fast A20 via Port 92h
        "in $0x92, %%al\n\t"
        "or $0x02, %%al\n\t"
        "out %%al, $0x92\n\t"
        : : : "ax"
    );
    puts("A20+\r\n");
    
    // Lade Preloader: 18 Sektoren ab LBA 1 (HDD LBA Mode)
    puts("Ld...\r\n");
    unsigned char int13_cf = 0;
    unsigned short int13_ax = 0;
    __asm__ volatile (
        "pushw %%es\n\t"
        "pushw %%bx\n\t"
        "pushw %%cx\n\t"
        "pushw %%dx\n\t"
        
        "mov $0x02, %%ah\n\t"    // Read Sectors (CHS)
        "mov $18, %%al\n\t"      // Count (18 Sektoren)
        "mov $0, %%ch\n\t"       // Cyl 0
        "mov $2, %%cl\n\t"       // Sec 2 (Start nach MBR)
        "mov $0, %%dh\n\t"       // Head 0
        "movb 0x7B00, %%dl\n\t"  // Drive von 0x7B00 laden
        "mov $0x1000, %%bx\n\t"  // ES:BX = 0x1000:0000
        "mov %%bx, %%es\n\t"
        "xor %%bx, %%bx\n\t"
        "int $0x13\n\t"
        
        "popw %%dx\n\t"
        "popw %%cx\n\t"
        "popw %%bx\n\t"
        "popw %%es\n\t"
        
        "setc %0\n\t"            // CF speichern
        "mov %%ax, %1\n\t"       // AX (Error Code) speichern
        : "=q"(int13_cf), "=a"(int13_ax)
        : // Keine Inputs (DL manuell geladen)
        : "memory", "cc"
    );

    if (int13_cf) {
        puts("Err: ");
        print_hex8((int13_ax >> 8) & 0xFF); // AH ausgeben (Error Code)
        puts("\r\n");
        for (;;) { __asm__("hlt"); }
    }

    // Springe zu 0x10000 (Segment 0x1000)
    __asm__ volatile ("ljmp $0x1000, $0x0000\n\t");
    
    // Sollte niemals erreicht werden
    for (;;) { __asm__("hlt"); }
}

// Die magische Boot-Signatur (muss bei Byte 510 stehen)
// Wir füllen den Rest mit Nullen auf, bis auf die letzten 2 Bytes
/* Boot-Signatur wird vom Linker-Skript an Byte 510 gesetzt. */