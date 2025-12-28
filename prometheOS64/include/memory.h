#ifndef MEMORY_H
#define MEMORY_H

void init_memory(unsigned int start, unsigned int size);
void* malloc(unsigned int size);
void free(void* ptr);
void print_memory_status();

#endif