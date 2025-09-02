#include "hdd.h"

#include "ports.h"
#include "debug.h"
#include "pci.h"

// Bus Master IDE I/O base will be detected via PCI (BAR4)
static uint16_t BM_COMMAND_REG = 0;
static uint16_t BM_STATUS_REG  = 0;
static uint16_t BM_PRDT_REG    = 0;

static void hdd_detect_bmio(void) {
    if (BM_COMMAND_REG) return;
    uint8_t bus = 0, slot = 0, func = 0;
    if (pci_find_device(0x01, 0x01, &bus, &slot, &func) == 0) {
        // ensure PCI command.bus master is enabled
        pci_enable_bus_master(bus, slot, func);

        uint32_t bar4 = pci_config_read_dword(bus, slot, func, 0x20);
        uint32_t bm_base = bar4 & ~0x3u;
        BM_COMMAND_REG = (uint16_t)(bm_base + 0);
        BM_STATUS_REG  = (uint16_t)(bm_base + 2);
        BM_PRDT_REG    = (uint16_t)(bm_base + 4);
        debug_puts("BM-IDE base "); debug_puthex(bm_base); debug_puts("\n");
    } else {
        // Fallback primary channel default on PIIX3
        BM_COMMAND_REG = 0x00C0;
        BM_STATUS_REG  = 0x00C2;
        BM_PRDT_REG    = 0x00C4;
        debug_puts("BM-IDE base fallback 0x00C0\n");
    }
}

typedef struct {
    uint32_t base;
    uint16_t count;
    uint16_t flags;
} __attribute__((packed)) prdt_t;

static prdt_t prdt __attribute__((aligned(16)));

static inline uint32_t virt_to_phys(void *addr) {
    return (uint32_t)addr; // identity mapping
}

#define MAX_PRDS 512   // reicht für 32 MiB
static prdt_t prdt_table[MAX_PRDS] __attribute__((aligned(16)));

static int g_dma_mode_ready = 0;

static int ata_set_udma_mode(void) {
    // Try to set Ultra DMA mode 2 via SET FEATURES (0xEF)
    // SectorCount = 0x20 | mode (mode 2 -> 0x22), Features = 0x03
    uint16_t command_port = 0x1F7;
    uint16_t device_port  = 0x1F6;
    uint16_t features_port = 0x1F1;
    uint16_t sector_count_port = 0x1F2;

    // Select drive 0, LBA mode
    while (port_byte_in(command_port) & 0x80);
    port_byte_out(device_port, 0xE0 | 0x40);

    while (port_byte_in(command_port) & 0x80);
    port_byte_out(features_port, 0x03);       // Set transfer mode
    port_byte_out(sector_count_port, 0x22);   // Ultra DMA mode 2
    port_byte_out(command_port, 0xEF);        // SET FEATURES

    // Wait for completion
    uint8_t st;
    do {
        st = port_byte_in(command_port);
    } while (st & 0x80);

    if (st & 0x01) { // ERR
        debug_puts("SET FEATURES UDMA2 failed, status=");
        debug_puthex(st);
        debug_puts("\n");
        return -1;
    }
    return 0;
}

int dma_read(uint32_t lba, void* buffer, uint32_t bytes) {
    debug_puts("dma_read start\n");

    if (bytes == 0) return 0;

    // Try DMA without forcing mode first; many environments already set DMA.

    uint32_t total_sectors = (bytes + 511) / 512;
    uint8_t* buf = (uint8_t*)buffer;

    while (total_sectors > 0) {
        // ATA allows max 256 sectors per command (0 means 256)
        uint32_t chunk_sectors = (total_sectors > 256) ? 256 : total_sectors;
        uint32_t chunk_bytes   = chunk_sectors * 512;

        // Build PRDT for this chunk: entries <=64KiB and not crossing 64KiB boundaries
        uint32_t remaining = chunk_bytes;
        uint8_t* cur = buf;
        int prd_count = 0;
        while (remaining > 0 && prd_count < MAX_PRDS) {
            uint32_t to_boundary = 0x10000 - ((uint32_t)cur & 0xFFFF);
            uint32_t piece = remaining;
            if (piece > 0x10000) piece = 0x10000;
            if (piece > to_boundary) piece = to_boundary;
            prdt_table[prd_count].base  = virt_to_phys(cur);
            prdt_table[prd_count].count = (piece == 0x10000) ? 0 : (uint16_t)piece; // 0 encodes 64KiB
            prdt_table[prd_count].flags = 0x0000;
            cur       += piece;
            remaining -= piece;
            prd_count++;
        }
        prdt_table[prd_count - 1].flags = 0x8000; // last entry

        // Program PRDT (detect BM base if needed)
        hdd_detect_bmio();
        port_long_out(BM_PRDT_REG, virt_to_phys(prdt_table));

        // Clear BM status (interrupt + error)
        port_byte_out(BM_STATUS_REG, 0x06);

        // Program ATA taskfile for READ DMA
        while (port_byte_in(0x1F7) & 0x80); // wait BSY=0
        port_byte_out(0x1F6, 0xE0 | ((lba >> 24) & 0x0F) | 0x40); // LBA + master
        port_byte_out(0x1F2, (chunk_sectors == 256) ? 0 : (chunk_sectors & 0xFF));
        port_byte_out(0x1F3, lba & 0xFF);
        port_byte_out(0x1F4, (lba >> 8) & 0xFF);
        port_byte_out(0x1F5, (lba >> 16) & 0xFF);
        port_byte_out(0x1F7, 0xC8); // READ DMA

        // Start BM: R/W=0 (read from device), Start=1
        uint8_t status;
retry_chunk:
        port_byte_out(BM_COMMAND_REG, 0x01);

        // Poll BM status: bit2=Interrupt, bit1=Error
        do {
            status = port_byte_in(BM_STATUS_REG);
        } while (!(status & 0x04) && !(status & 0x02));

        // Stop and acknowledge interrupt
        port_byte_out(BM_COMMAND_REG, 0x00);
        port_byte_out(BM_STATUS_REG, status | 0x04);

        debug_puts("dma_read chunk status=");
        debug_puthex(status);
        debug_puts("\n");

        if (status & 0x02) {
            uint8_t ata_st = port_byte_in(0x1F7);
            uint8_t ata_er = port_byte_in(0x1F1);
            debug_puts(" ATA ST="); debug_puthex(ata_st);
            debug_puts(" ER="); debug_puthex(ata_er);
            debug_puts("\n");
            if (!g_dma_mode_ready) {
                // Try enabling UDMA2 once and retry this chunk
                if (ata_set_udma_mode() == 0) {
                    g_dma_mode_ready = 1;
                    // Clear status again before retry
                    port_byte_out(BM_STATUS_REG, 0x06);
                    goto retry_chunk;
                }
            }
            // Give up on DMA: fall back entire transfer to PIO fast for robustness
            debug_puts("DMA failed; fallback to PIO fast for remaining\n");
            read_from_disk_fast(lba, buf, total_sectors * 512);
            return 0;
        }

        // Advance
        lba           += chunk_sectors;
        buf           += chunk_bytes;
        total_sectors -= chunk_sectors;
    }

    return 0;
}


int dma_write(uint32_t lba, void* buffer, uint32_t bytes) {
    debug_puts("dma_write start lba=");
    debug_puthex(lba);
    debug_puts(" bytes=");
    debug_puthex(bytes);
    debug_puts("\n");

    // Ensure BM base is known
    hdd_detect_bmio();

    prdt.base = virt_to_phys(buffer);
    prdt.count = bytes;
    prdt.flags = 0x8000;

    port_long_out(BM_PRDT_REG, virt_to_phys(&prdt));
    port_byte_out(BM_STATUS_REG, 0x06);

    uint16_t sector_count = (bytes + 511) / 512;
    uint16_t sector_count_port = 0x1F2;
    uint16_t lba_low_port = 0x1F3;
    uint16_t lba_mid_port = 0x1F4;
    uint16_t lba_high_port = 0x1F5;
    uint16_t device_port = 0x1F6;
    uint16_t command_port = 0x1F7;

    while (port_byte_in(command_port) & 0x80);

    port_byte_out(device_port, 0xE0 | ((lba >> 24) & 0x0F));
    port_byte_out(sector_count_port, sector_count);
    port_byte_out(lba_low_port, lba & 0xFF);
    port_byte_out(lba_mid_port, (lba >> 8) & 0xFF);
    port_byte_out(lba_high_port, (lba >> 16) & 0xFF);
    port_byte_out(command_port, 0xCA); // WRITE DMA

    // Start BM: R/W=1 (write to device), Start=1
    port_byte_out(BM_COMMAND_REG, 0x03);

    uint8_t status;
    do {
        status = port_byte_in(BM_STATUS_REG);
    } while (!(status & 0x04) && !(status & 0x02));

    port_byte_out(BM_COMMAND_REG, 0x00);
    port_byte_out(BM_STATUS_REG, status | 0x04);

    debug_puts("dma_write end status=");
    debug_puthex(status);
    debug_puts("\n");

    if (status & 0x02)
        return -1;
    return 0;
}


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

void read_from_disk_fast(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes) {
    uint32_t total_count = (size_in_bytes + 511) / 512;
    uint16_t data_port = 0x1F0;
    uint16_t sector_count_port = 0x1F2;
    uint16_t lba_low_port = 0x1F3;
    uint16_t lba_mid_port = 0x1F4;
    uint16_t lba_high_port = 0x1F5;
    uint16_t device_port = 0x1F6;
    uint16_t command_port = 0x1F7;

    uint16_t* buffer16 = (uint16_t*)buffer;

    debug_puts("read_from_disk_fast\n");

    while (total_count > 0) {
        uint16_t sectors = total_count > 128 ? 128 : total_count;

        debug_puts(" lba=");
        debug_puthex(lba);
        debug_puts(" sectors=");
        debug_puthex(sectors);
        debug_puts("\n");

        while ((port_byte_in(command_port) & 0x80) != 0);

        port_byte_out(device_port, 0xE0 | ((lba >> 24) & 0x0F) | 0x40);
        port_byte_out(sector_count_port, sectors);
        port_byte_out(lba_low_port, lba & 0xFF);
        port_byte_out(lba_mid_port, (lba >> 8) & 0xFF);
        port_byte_out(lba_high_port, (lba >> 16) & 0xFF);
        port_byte_out(command_port, 0x20);

        for (uint16_t i = 0; i < sectors; i++) {
            while ((port_byte_in(command_port) & 0x08) == 0);
            uint32_t words = 256;
            __asm__ volatile("rep insw" : "+D"(buffer16), "+c"(words) : "d"(data_port) : "memory");
            while ((port_byte_in(command_port) & 0x80) != 0);
        }

        lba += sectors;
        total_count -= sectors;
        buffer16 += sectors * 256;
    }
}

void write_to_disk_fast(uint32_t lba, uint8_t* buffer, uint32_t size_in_bytes) {
    uint32_t count = (size_in_bytes + 511) / 512;
    uint16_t data_port = 0x1F0;
    uint16_t sector_count_port = 0x1F2;
    uint16_t lba_low_port = 0x1F3;
    uint16_t lba_mid_port = 0x1F4;
    uint16_t lba_high_port = 0x1F5;
    uint16_t device_port = 0x1F6;
    uint16_t command_port = 0x1F7;

    while ((port_byte_in(command_port) & 0x80) != 0);

    port_byte_out(device_port, 0xE0 | ((lba >> 24) & 0x0F));
    port_byte_out(sector_count_port, count);
    port_byte_out(lba_low_port, lba & 0xFF);
    port_byte_out(lba_mid_port, (lba >> 8) & 0xFF);
    port_byte_out(lba_high_port, (lba >> 16) & 0xFF);
    port_byte_out(command_port, 0x30);

    uint16_t* buffer16 = (uint16_t*)buffer;
    for (uint32_t i = 0; i < count; i++) {
        while ((port_byte_in(command_port) & 0x08) == 0);
        uint32_t words = 256;
        __asm__ volatile("rep outsw" : "+S"(buffer16), "+c"(words) : "d"(data_port) : "memory");
        while ((port_byte_in(command_port) & 0x80) != 0);
    }
}
