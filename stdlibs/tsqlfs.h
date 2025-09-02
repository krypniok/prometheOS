#pragma once

#include <stddef.h>

typedef struct {
    char name[32];
    unsigned char* buffer;
    size_t size;
    size_t capacity;
    size_t pos;
    char mode;
} TSQLFILE;

TSQLFILE* tsql_fopen(const char* name, const char* mode);
size_t tsql_fread(void* ptr, size_t size, size_t count, TSQLFILE* stream);
size_t tsql_fwrite(const void* ptr, size_t size, size_t count, TSQLFILE* stream);
int tsql_fclose(TSQLFILE* stream);

int tsqlfs_test();

