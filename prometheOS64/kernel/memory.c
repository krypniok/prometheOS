#include "../include/memory.h"
#include "../include/drivers/console.h"

// Header für jeden Speicherblock
typedef struct block_header {
    unsigned int size;        // Größe der Nutzdaten (ohne Header)
    unsigned int is_free;     // 1 = Frei, 0 = Belegt
    struct block_header *next; // Zeiger auf den nächsten Block
} block_header_t;

static block_header_t *head = 0;

// Initialisiert den Heap an einer festen Adresse
void init_memory(unsigned int start, unsigned int size) {
    head = (block_header_t*)start;
    head->size = size - sizeof(block_header_t);
    head->is_free = 1;
    head->next = 0;
}

void* malloc(unsigned int size) {
    if (size == 0) return 0;

    // 4-Byte Alignment erzwingen (wichtig für CPU-Performance)
    if (size % 4 != 0) {
        size += 4 - (size % 4);
    }

    block_header_t *curr = head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Block teilen, wenn er viel zu groß ist (Split)
            // Wir brauchen Platz für Header + Nutzdaten + min. 4 Bytes Rest
            if (curr->size > size + sizeof(block_header_t) + 4) {
                block_header_t *new_block = (block_header_t*)((char*)curr + sizeof(block_header_t) + size);
                new_block->size = curr->size - size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }
            curr->is_free = 0;
            // Zeiger auf den Datenbereich zurückgeben (hinter dem Header)
            return (void*)(curr + 1);
        }
        curr = curr->next;
    }
    return 0; // Out of memory (kein passender Block gefunden)
}

void free(void* ptr) {
    if (!ptr) return;

    // Header liegt direkt vor dem Pointer
    block_header_t *header = (block_header_t*)ptr - 1;
    header->is_free = 1;

    // Aufräumen: Benachbarte freie Blöcke verschmelzen (Coalescing)
    // Wir scannen die Liste von vorne (einfach, aber O(N))
    block_header_t *curr = head;
    while (curr) {
        if (curr->is_free && curr->next && curr->next->is_free) {
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
            // Nicht weiterschalten, um evtl. noch den übernächsten zu verschmelzen
        } else {
            curr = curr->next;
        }
    }
}

void print_memory_status() {
    term_puts("Heap Status:\n");
    block_header_t *curr = head;
    unsigned int total_free = 0;
    unsigned int total_used = 0;
    unsigned int blocks = 0;
    
    while (curr) {
        if (curr->is_free) total_free += curr->size;
        else total_used += curr->size;
        blocks++;
        curr = curr->next;
    }
    
    term_puts(" Blocks: "); term_putdec(blocks); term_puts("\n");
    term_puts(" Free:   "); term_putdec(total_free); term_puts(" bytes\n");
    term_puts(" Used:   "); term_putdec(total_used); term_puts(" bytes\n");
}