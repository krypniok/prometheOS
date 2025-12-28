#include <stddef.h>
#include "file.h"
#include "memory.h"
#include "string.h"
#include "../drivers/hdd.h"

RAMFILE ramdisk[MAX_FILES];
int numFiles = 0;


RAMFILE *ramdisk_fopen(const char *filename, const char *mode) {
    // Überprüfen, ob der Dateiname bereits vorhanden ist
    for (int i = 0; i < numFiles; i++) {
        if (strcmp(filename, ramdisk[i].filename) == 0) {
            if (strcmp(mode, "r") == 0 || strcmp(mode, "a") == 0) {
                // Wenn im Lesemodus oder Anhängemodus geöffnet wird,
                // gibt die vorhandene Datei zurück, ohne ihren Inhalt zu löschen.
                return &ramdisk[i];
            } else if (strcmp(mode, "w") == 0) {
                // Wenn im Schreibmodus (w) geöffnet wird, lösche den Inhalt der Datei.
                RAMFILE *file = &ramdisk[i];
                file->size = 0;
                return file;
            }
        }
    }

    if (numFiles >= MAX_FILES) {
        printf("Max number of files exceeded.\n");
        return NULL;
    }

    if (strcmp(mode, "r") == 0 || strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0) {
        RAMFILE *file = &ramdisk[numFiles++];
        strncpy(file->filename, filename, sizeof(file->filename));
        file->size = 0;
        file->content = (char *)calloc(MAX_FILE_SIZE, sizeof(char));
        file->mode = mode[0]; // Nehmen Sie den ersten Buchstaben des Modus-Strings
        return file;
    }

    printf("Invalid mode: %s\n", mode);
    return NULL;
}

size_t ramdisk_fread(void *ptr, size_t size, size_t count, RAMFILE *stream) {
    RAMFILE *file = (RAMFILE *)stream;
    size_t bytesRead = size * count;
    if (file->size < bytesRead) {
        bytesRead = file->size;
    }
    memcpy(ptr, file->content, bytesRead);
    return bytesRead;
}

size_t ramdisk_fwrite(const void *ptr, size_t size, size_t count, RAMFILE *stream) {
    RAMFILE *file = (RAMFILE *)stream;
    size_t bytesWritten = size * count;
    if (file->size + bytesWritten <= MAX_FILE_SIZE) {
        memcpy(file->content + file->size, ptr, bytesWritten);
        file->size += bytesWritten;
    } else {
        printf("File size exceeds maximum limit.\n");
        bytesWritten = 0;
    }
    return bytesWritten;
}

int ramdisk_fclose(RAMFILE *stream) {
    free(stream->content);
    return 0;
}

size_t ramdisk_filesize(RAMFILE *stream) {
    return stream->size;
}

int ramdisk_eof(RAMFILE *stream) {
    return (stream->size == 0);
}

int ramdisk_delete(const char *filename) {
    for (int i = 0; i < numFiles; i++) {
        if (strcmp(filename, ramdisk[i].filename) == 0) {
            free(ramdisk[i].content);
            for (int j = i; j < numFiles - 1; j++) {
                ramdisk[j] = ramdisk[j + 1];
            }
            numFiles--;
            return 1;
        }
    }
    return 0; // Datei nicht gefunden
}

void ramdisk_list_files() {
    printf("List of files in RAM Disk:\n");
    for (int i = 0; i < numFiles; i++) {
        RAMFILE *file = &ramdisk[i];
        printf("Filename: %s ", file->filename);
        printf("Length: %d ", file->size);
        printf("Address: %p\n", (void *)file);
    }
}

int ramdisk_test() {
    RAMFILE *file = ramdisk_fopen("example.txt", "w");
    if (file) {
        unsigned char *text = "Hello, RAM Disk!";
        ramdisk_fwrite(text, sizeof(char), strlen(text), file);
        ramdisk_fclose(file);
    }

    file = ramdisk_fopen("example.txt", "r");
    if (file) {
        unsigned char buffer[100];
        memset(&buffer, 0, 100);
        ramdisk_fread(buffer, sizeof(char), file->size, file);
        ramdisk_fclose(file);
        printf("Read from RAM Disk: %s\n", buffer);
    }
/*
    // Löschen einer Datei
    int deleteResult = ramdisk_delete("example.txt");
    if (deleteResult) {
        printf("File 'example.txt' deleted.\n");
    } else {
        printf("File 'example.txt' not found.\n");
    }
*/
    // Überprüfen, ob die Datei 'example.txt' nach dem Löschen noch vorhanden ist
    ramdisk_list_files();

    return 0;
}

#define RDISK_MAGIC 0x52444B31u /* 'RDK1' */

int ramdisk_save_image(uint32_t lba_start) {
    // Header sector: magic, version(1), numFiles
    uint8_t* sector = (uint8_t*)calloc(512, 1);
    if (!sector) return -1;
    uint32_t* w = (uint32_t*)sector;
    w[0] = RDISK_MAGIC;
    w[1] = 1; // version
    w[2] = (uint32_t)numFiles;
    if (dma_write(lba_start, sector, 512) != 0) { free(sector); return -2; }
    free(sector);

    uint32_t lba = lba_start + 1;
    for (int i = 0; i < numFiles; i++) {
        RAMFILE* f = &ramdisk[i];
        // write file header sector: name[32], size(uint32)
        sector = (uint8_t*)calloc(512, 1);
        if (!sector) return -3;
        memcpy(sector, f->filename, sizeof(f->filename));
        uint32_t* szp = (uint32_t*)(sector + 32);
        *szp = (uint32_t)f->size;
        if (dma_write(lba, sector, 512) != 0) { free(sector); return -4; }
        free(sector);
        lba += 1;

        // write content padded to 512
        uint32_t to_write = (uint32_t)f->size;
        uint32_t padded = (to_write + 511) & ~511u;
        uint8_t* buf = (uint8_t*)calloc(padded, 1);
        if (!buf) return -5;
        if (to_write) memcpy(buf, f->content, to_write);
        if (dma_write(lba, buf, padded) != 0) { free(buf); return -6; }
        free(buf);
        lba += padded / 512;
    }

    return 0;
}

int ramdisk_load_image(uint32_t lba_start) {
    // reset existing
    for (int i = 0; i < numFiles; i++) {
        if (ramdisk[i].content) free(ramdisk[i].content);
    }
    numFiles = 0;

    uint8_t* sector = (uint8_t*)calloc(512, 1);
    if (!sector) return -1;
    if (dma_read(lba_start, sector, 512) != 0) { free(sector); return -2; }
    uint32_t* r = (uint32_t*)sector;
    if (r[0] != RDISK_MAGIC || r[1] != 1) { free(sector); return -3; }
    uint32_t files = r[2];
    free(sector);

    uint32_t lba = lba_start + 1;
    for (uint32_t i = 0; i < files && numFiles < MAX_FILES; i++) {
        sector = (uint8_t*)calloc(512, 1);
        if (!sector) return -4;
        if (dma_read(lba, sector, 512) != 0) { free(sector); return -5; }
        char name[32];
        memcpy(name, sector, 32);
        uint32_t size = *(uint32_t*)(sector + 32);
        free(sector);
        lba += 1;

        RAMFILE* f = &ramdisk[numFiles++];
        memset(f, 0, sizeof(RAMFILE));
        strncpy(f->filename, name, sizeof(f->filename));
        f->size = 0;
        f->content = (char*)calloc(MAX_FILE_SIZE, 1);
        f->mode = 'r';

        if (size) {
            uint32_t padded = (size + 511) & ~511u;
            uint8_t* buf = (uint8_t*)calloc(padded, 1);
            if (!buf) return -6;
            if (dma_read(lba, buf, padded) != 0) { free(buf); return -7; }
            // clamp to MAX_FILE_SIZE
            uint32_t cp = size;
            if (cp > MAX_FILE_SIZE) cp = MAX_FILE_SIZE;
            memcpy(f->content, buf, cp);
            f->size = cp;
            free(buf);
            lba += padded / 512;
        }
    }

    return 0;
}

// Convenience wrappers for kernel console without args
// Use LBA 8192 (4 MiB) to avoid collision with assets at 2 MiB
int ramdisk_saveimg() { return ramdisk_save_image(8192); }
int ramdisk_loadimg() { return ramdisk_load_image(8192); }
