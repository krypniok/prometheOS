#include "memory.h"

// Der flache Speicherbereich, in dem wir unsere Allokationen vornehmen werden.
char memory[MEMORY_SIZE];

void memcpy(void* dest, const void* src, size_t numBytes) {
    char* destPtr = (char*)dest;
    const char* srcPtr = (const char*)src;

    if (destPtr < srcPtr) {
        while (numBytes--)
            *destPtr++ = *srcPtr++;
    } else {
        destPtr += numBytes;
        srcPtr += numBytes;
        while (numBytes--)
            *--destPtr = *--srcPtr;
    }
}

void memset(void* ptr, int value, size_t numBytes) {
    char* bytePtr = (char*)ptr;
    char byteValue = (char)value;

    while (numBytes--)
        *bytePtr++ = byteValue;
}

int memcmp(const void* ptr1, const void* ptr2, size_t numBytes) {
    const unsigned char* bytePtr1 = (const unsigned char*)ptr1;
    const unsigned char* bytePtr2 = (const unsigned char*)ptr2;

    for (size_t i = 0; i < numBytes; i++) {
        if (bytePtr1[i] != bytePtr2[i])
            return bytePtr1[i] - bytePtr2[i];
    }

    return 0;
}

// Struktur zur Verwaltung von Speicherblöcken
typedef struct Block {
    size_t size;
    struct Block* next;
} Block;

// Der Anfang des freien Speichers
Block* free_list = NULL;

// Funktion zum Initialisieren der Speicherverwaltung
void init_memory() {
    free_list = (Block*)memory;
    free_list->size = MEMORY_SIZE - sizeof(Block);
    free_list->next = NULL;
}

// Funktion zum Allozieren von Speicher
void* malloc(size_t size) {
    // Größe des Allokationsblocks auf die nächste gerade Zahl aufrunden,
    // um eine ausgerichtete Speicherzuteilung zu gewährleisten.
    size = (size + 1) & ~1;

    Block* prev = NULL;
    Block* curr = free_list;

    // Durchsuchen der freien Liste, um einen geeigneten Block zu finden
    while (curr) {
        if (curr->size >= size) {
            // Wenn der Block groß genug ist, um die gewünschte Größe aufzunehmen,
            // nehmen wir ihn und teilen ihn auf, wenn möglich.
            if (curr->size >= size + sizeof(Block) + 2) {
                Block* next_block = (Block*)((char*)curr + sizeof(Block) + size);
                next_block->size = curr->size - size - sizeof(Block);
                next_block->next = curr->next;
                curr->size = size;
                curr->next = next_block;
            }

            // Den gefundenen Block aus der freien Liste entfernen.
            if (prev) {
                prev->next = curr->next;
            } else {
                free_list = curr->next;
            }

            return (char*)curr + sizeof(Block);
        }

        prev = curr;
        curr = curr->next;
    }

    // Wenn kein passender Block gefunden wurde, geben wir NULL zurück.
    return NULL;
}

// Funktion zum Freigeben von Speicher
void free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    // Der übergebene Zeiger zeigt auf den Anfang des vom Nutzer angeforderten Blocks.
    // Wir müssen ihn auf den Anfang des Block-Headers verschieben.
    Block* block = (Block*)((char*)ptr - sizeof(Block));

    // Füge den Block wieder in die freie Liste ein.
    block->next = free_list;
    free_list = block;
}

// Funktion zum Ändern der Größe eines allokierten Speicherblocks
void* realloc(void* ptr, size_t new_size) {
    if (ptr == NULL) {
        // If ptr is NULL, it's equivalent to malloc(new_size)
        return malloc(new_size);
    }
    if (new_size == 0) {
        // If new_size is 0, it's equivalent to free(ptr) and returns NULL
        free(ptr);
        return NULL;
    }

    // Move the pointer to the block header
    Block* block = (Block*)((char*)ptr - sizeof(Block));

    // If the existing block is large enough to accommodate the new size
    if (block->size >= new_size) {
        // Check if we can split the block
        if (block->size >= new_size + sizeof(Block) + 2) {
            Block* next_block = (Block*)((char*)block + sizeof(Block) + new_size);
            next_block->size = block->size - new_size - sizeof(Block);
            next_block->next = block->next;
            block->size = new_size;
            block->next = next_block;
        }
        return ptr;
    }

    // Allocate a new block of the desired size
    void* new_ptr = malloc(new_size);
    if (new_ptr) {
        // Copy the data from the old block to the new block
        memcpy(new_ptr, ptr, block->size);
        // Free the old block
        free(ptr);
    }
    return new_ptr;
}

// Funktion zum Auflisten aller allokierten Blöcke
void list_allocated_blocks() {
    Block* curr = free_list;

    while (curr) {
        printf("Block at %p, ", (void*)curr);
        printf("Size: %d\n", curr->size);
        curr = curr->next;
    }
}

int memtest() {
    void* mem = (void*)malloc(256);
    void* mem2 = (void*)malloc(128);
    printf("Allocated blocks:\n");
    list_allocated_blocks();
    free(mem);
    printf("\nAllocated blocks after freeing mem:\n");
    list_allocated_blocks();
    free(mem2);
    printf("\nAllocated blocks after freeing mem2:\n");
    list_allocated_blocks();
    return 0;
}
