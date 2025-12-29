#include "../../include/io.h"
#include "../../include/drivers/ata.h"

// --- ATA Driver (PIO Mode) ---
void ata_read_sector(unsigned int lba, unsigned char *buffer) {
    // Drive Select (Master 0xE0) + LBA High 4 bits
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F1, 0x00); // Null
    outb(0x1F2, 1);    // Sector Count = 1
    outb(0x1F3, (unsigned char)lba);
    outb(0x1F4, (unsigned char)(lba >> 8));
    outb(0x1F5, (unsigned char)(lba >> 16));
    outb(0x1F7, 0x20); // Command: Read Sectors with Retry

    // Wait for BSY to clear and DRQ to set
    while (1) {
        unsigned char status = inb(0x1F7);
        if (!(status & 0x80) && (status & 0x08)) break;
        if (status & 0x01) { // Error
            return; // Fehlerbehandlung später via Return-Code
        }
    }

    // Read 256 words (512 bytes)
    insw(0x1F0, buffer, 256);
}