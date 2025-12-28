#include <stdint.h>

// Wir nutzen die Ports direkt, um Abhängigkeiten gering zu halten
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void insw(uint16_t port, void *addr, uint32_t cnt) {
    asm volatile ( "cld; rep insw" : "+D"(addr), "+c"(cnt) : "d"(port) : "memory");
}

// VGA Ausgabe für Statusmeldungen
void print_k(const char* str) {
    static uint16_t* vidmem = (uint16_t*)0xB8000;
    static int offset = 0;
    
    while (*str) {
        vidmem[offset++] = (0x0F << 8) | *str++;
    }
    // Zeilenumbruch simulieren (einfach 80 addieren für nächste Zeile, grob)
    offset = (offset + 80) - (offset % 80);
}

// ATA PIO Read
#define ATA_DATA        0x1F0
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_CMD_READ    0x20

void ata_read_sector(uint32_t lba, uint8_t* buffer) {
    // Wait for drive to be ready (BSY clear, DRDY set)
    while ((inb(ATA_STATUS) & 0xC0) != 0x40);

    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,  (uint8_t)(lba >> 16));
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_STATUS, ATA_CMD_READ);

    // Wait for DRQ
    while ((inb(ATA_STATUS) & 0x88) != 0x08);

    // Read 256 words (512 bytes)
    insw(ATA_DATA, buffer, 256);
}

void boot_main(void) {
    print_k("PrometheOS Bootloader (C-Mode) started...");

    // Kernel laden
    // LBA 130 ist der Start des Kernels laut Makefile
    // Wir laden 4MB (8192 Sektoren) sicherheitshalber an 0x100000 (1MB Mark)
    uint8_t* kernel_dest = (uint8_t*)0x100000;
    uint32_t start_sector = 130;
    uint32_t sectors_to_load = 8192; 

    print_k("Loading Kernel from Disk...");
    for (uint32_t i = 0; i < sectors_to_load; i++) {
        ata_read_sector(start_sector + i, kernel_dest + (i * 512));
    }

    print_k("Launching Kernel...");
    
    // Sprung zum Kernel Entry Point (0x100000)
    void (*kernel_entry)(void) = (void (*)(void))0x100000;
    kernel_entry();
}