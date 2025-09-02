#include "string.h"
#include "memory.h"
#include "tsqlfs.h"
#include "homebrewdb.h"

#define TSQLFS_INIT_CAP 256

static void ensure_capacity(TSQLFILE* f, size_t need_end) {
    if (need_end <= f->capacity) return;
    size_t newcap = f->capacity ? f->capacity : TSQLFS_INIT_CAP;
    while (newcap < need_end) newcap *= 2;
    unsigned char* nb = (unsigned char*)realloc(f->buffer, newcap);
    if (!nb) return; // out of memory; in kernel just leave as is
    // zero-fill the new region
    if (newcap > f->capacity) memset(nb + f->capacity, 0, newcap - f->capacity);
    f->buffer = nb;
    f->capacity = newcap;
}

TSQLFILE* tsql_fopen(const char* name, const char* mode) {
    hbdb_ensure_fs_table();
    TSQLFILE* f = (TSQLFILE*)malloc(sizeof(TSQLFILE));
    if (!f) return 0;
    memset(f, 0, sizeof(TSQLFILE));
    strncpy(f->name, name, sizeof(f->name));
    f->mode = mode && mode[0] ? mode[0] : 'r';

    if (f->mode == 'w') {
        f->capacity = TSQLFS_INIT_CAP;
        f->buffer = (unsigned char*)calloc(f->capacity, 1);
        f->size = 0;
        f->pos = 0;
        return f;
    }

    // Try to load existing content
    unsigned char* data = 0; size_t sz = 0;
    if (hbdb_fs_get(f->name, &data, &sz)) {
        f->buffer = (unsigned char*)malloc(sz > 0 ? sz : TSQLFS_INIT_CAP);
        if (!f->buffer) { free(f); return 0; }
        f->capacity = (sz > 0) ? sz : TSQLFS_INIT_CAP;
        if (sz) memcpy(f->buffer, data, sz);
        f->size = sz;
        free(data);
    } else {
        // File not found
        if (f->mode == 'r') { free(f); return 0; }
        // For append on non-existing, create empty
        f->capacity = TSQLFS_INIT_CAP;
        f->buffer = (unsigned char*)calloc(f->capacity, 1);
        f->size = 0;
    }

    f->pos = (f->mode == 'a') ? f->size : 0;
    return f;
}

size_t tsql_fread(void* ptr, size_t size, size_t count, TSQLFILE* stream) {
    if (!stream || !ptr) return 0;
    size_t bytes = size * count;
    size_t remain = (stream->pos < stream->size) ? (stream->size - stream->pos) : 0;
    if (bytes > remain) bytes = remain;
    if (bytes == 0) return 0;
    memcpy(ptr, stream->buffer + stream->pos, bytes);
    stream->pos += bytes;
    return bytes;
}

size_t tsql_fwrite(const void* ptr, size_t size, size_t count, TSQLFILE* stream) {
    if (!stream || !ptr) return 0;
    if (stream->mode == 'r') return 0;
    size_t bytes = size * count;
    size_t endpos = stream->pos + bytes;
    ensure_capacity(stream, endpos);
    memcpy(stream->buffer + stream->pos, ptr, bytes);
    stream->pos += bytes;
    if (stream->pos > stream->size) stream->size = stream->pos;
    return bytes;
}

int tsql_fclose(TSQLFILE* stream) {
    if (!stream) return -1;
    // Commit back to TinySQL table
    hbdb_fs_put(stream->name, stream->buffer, stream->size);
    // Optional autosave to image, but only if this was a write/append stream.
    // Avoid expensive full-DB saves after simple read-only closes.
    if (stream->mode != 'r') {
        hbdb_autosave_maybe();
    }
    free(stream->buffer);
    free(stream);
    return 0;
}

int tsqlfs_test() {
    // Write
    TSQLFILE* f = tsql_fopen("hello.txt", "w");
    if (!f) { printf("tsqlfs: open failed\n"); return -1; }
    const char* msg = "Hello from TinySQL FS!";
    tsql_fwrite(msg, 1, strlen(msg), f);
    tsql_fclose(f);

    // Read
    f = tsql_fopen("hello.txt", "r");
    if (!f) { printf("tsqlfs: reopen failed\n"); return -1; }
    unsigned char buf[64];
    memset(buf, 0, sizeof(buf));
    tsql_fread(buf, 1, sizeof(buf)-1, f);
    tsql_fclose(f);
    printf("tsqlfs read: %s\n", buf);
    return 0;
}
