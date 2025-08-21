#include <stddef.h>
#include "file.h"
#include "memory.h"

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
