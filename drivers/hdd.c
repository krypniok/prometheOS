#include "hdd.h"

#include "ports.h"

// Funktion zum Lesen von Daten von der Festplatte
void read_from_disk(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes) {
    uint16_t* buffer16 = (uint16_t*)buffer;
    uint32_t count = (size_in_bytes + 511) / 512; // Aufrunden auf ganze Sektoren

    // Port-Nummern für den IDE-Controller (Primary)
    uint16_t data_port = 0x1F0;     // Datenport
    uint16_t error_port = 0x1F1;    // Fehlerport
    uint16_t sector_count_port = 0x1F2; // Anzahl der Sektoren
    uint16_t lba_low_port = 0x1F3;  // LBA-Adressregister (Niedrige Bits)
    uint16_t lba_mid_port = 0x1F4;  // LBA-Adressregister (Mittlere Bits)
    uint16_t lba_high_port = 0x1F5; // LBA-Adressregister (Hohe Bits)
    uint16_t device_port = 0x1F6;   // Geräte/Auswahlregister
    uint16_t command_port = 0x1F7;  // Befehls-/Statusport

    // Warten, bis der Controller bereit ist
    while ((port_byte_in(command_port) & 0x80) != 0);

    port_byte_out(device_port, 0xE0 | ((lba >> 24) & 0x0F) | 0x40); // Lesebefehl für LBA (bit 6 auf 1 setzen)
    port_byte_out(sector_count_port, count);
    port_byte_out(lba_low_port, lba & 0xFF);
    port_byte_out(lba_mid_port, (lba >> 8) & 0xFF);
    port_byte_out(lba_high_port, (lba >> 16) & 0xFF);
    port_byte_out(command_port, 0x20); // Befehl zum Lesen von Sektoren

    // Warten, bis der Controller bereit ist
    while ((port_byte_in(command_port) & 0x80) != 0);

    // Daten von der Festplatte in den Speicher kopieren
    for (uint32_t i = 0; i < count; i++) {
        for (uint16_t j = 0; j < 256; j++) {
            buffer16[j] = port_word_in(data_port);
        }
        buffer16 += 256;
        while ((port_byte_in(command_port) & 0x80) != 0);
    }
}

// Funktion zum Schreiben von Daten auf die Festplatte
void write_to_disk(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes) {
    uint16_t* buffer16 = (uint16_t*)buffer;
    uint32_t count = (size_in_bytes + 511) / 512; // Aufrunden auf ganze Sektoren
    printf("Sector Count: %d\n", count);
    // Port-Nummern für den IDE-Controller (Primary)
    uint16_t data_port = 0x1F0;     // Datenport
    uint16_t error_port = 0x1F1;    // Fehlerport
    uint16_t sector_count_port = 0x1F2; // Anzahl der Sektoren
    uint16_t lba_low_port = 0x1F3;  // LBA-Adressregister (Niedrige Bits)
    uint16_t lba_mid_port = 0x1F4;  // LBA-Adressregister (Mittlere Bits)
    uint16_t lba_high_port = 0x1F5; // LBA-Adressregister (Hohe Bits)
    uint16_t device_port = 0x1F6;   // Geräte/Auswahlregister
    uint16_t command_port = 0x1F7;  // Befehls-/Statusport

    // Warten, bis der Controller bereit ist
    while ((port_byte_in(command_port) & 0x80) != 0);

    // Senden Sie den Befehl zum Schreiben
    port_byte_out(device_port, 0xE0 | ((lba >> 24) & 0x0F)); // Schreibbefehl für LBA
    port_byte_out(sector_count_port, count);
    port_byte_out(lba_low_port, lba & 0xFF);
    port_byte_out(lba_mid_port, (lba >> 8) & 0xFF);
    port_byte_out(lba_high_port, (lba >> 16) & 0xFF);
    port_byte_out(command_port, 0x30); // Befehl zum Schreiben von Sektoren

    // Warten, bis der Controller bereit ist
    while ((port_byte_in(command_port) & 0x80) != 0);

    // Daten von Speicher auf die Festplatte kopieren
    for (uint32_t i = 0; i < count; i++) {
        for (uint16_t j = 0; j < 256; j++) {
            port_word_out(data_port, buffer16[j]);
        }
        buffer16 += 256;
        while ((port_byte_in(command_port) & 0x80) != 0);
    }
}
