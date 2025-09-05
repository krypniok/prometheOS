#include "string.h"
#include "memory.h"
#include "tsqlfs.h"
#include "homebrewdb.h"

#define TSQLFS_INIT_CAP 256

// Ensure buffer can hold need_end bytes. Returns 1 on success, 0 on OOM.
static int ensure_capacity(TSQLFILE* f, size_t need_end) {
    if (need_end <= f->capacity) return 1;
    size_t newcap = f->capacity ? f->capacity : TSQLFS_INIT_CAP;
    while (newcap < need_end) {
        size_t next = newcap * 2;
        if (next <= newcap) { // overflow guard
            next = need_end;
        }
        newcap = next;
    }
    unsigned char* nb = (unsigned char*)realloc(f->buffer, newcap);
    if (!nb) return 0; // out of memory; preserve old buffer
    // zero-fill the new region
    if (newcap > f->capacity) memset(nb + f->capacity, 0, newcap - f->capacity);
    f->buffer = nb;
    f->capacity = newcap;
    return 1;
}

static int is_blank(const char* s){ if(!s) return 1; while(*s){ if(*s!=' '&&*s!='\t') return 0; s++; } return 1; }
static void trim_name(const char** pin, char* out, size_t outsz){
    const char* in = *pin ? *pin : "";
    // ltrim
    while (*in==' '||*in=='\t') in++;
    // copy with limit
    size_t n = strlen(in); if (n >= outsz) n = outsz-1;
    memcpy(out, in, n); out[n]='\0';
    // rtrim
    while (n>0 && (out[n-1]==' '||out[n-1]=='\t')) { out[--n]='\0'; }
}

TSQLFILE* tsql_fopen(const char* name, const char* mode) {
    // reject empty/null or blank names to avoid creating invisible entries
    if (!name || !name[0] || is_blank(name)) return 0;
    hbdb_ensure_fs_table();
    TSQLFILE* f = (TSQLFILE*)malloc(sizeof(TSQLFILE));
    if (!f) return 0;
    memset(f, 0, sizeof(TSQLFILE));
    // copy trimmed name
    trim_name(&name, f->name, sizeof(f->name));
    if (f->name[0]=='\0') { free(f); return 0; }
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
    if (!ensure_capacity(stream, endpos)) {
        // write as much as fits to avoid memory corruption
        if (stream->pos >= stream->capacity) return 0;
        size_t writable = stream->capacity - stream->pos;
        if (writable == 0) return 0;
        memcpy(stream->buffer + stream->pos, ptr, writable);
        stream->pos += writable;
        if (stream->pos > stream->size) stream->size = stream->pos;
        return writable / size; // return count-partial in items
    }
    memcpy(stream->buffer + stream->pos, ptr, bytes);
    stream->pos += bytes;
    if (stream->pos > stream->size) stream->size = stream->pos;
    return count;
}

int tsql_fclose(TSQLFILE* stream) {
    if (!stream) return -1;
    // Skip invalid/empty-name files to avoid creating "" entries
    if (stream->name[0] != '\0') {
        // Commit back to TinySQL table
        // If this was a write stream but size==0 and you don't want empty files,
        // you could skip here; by default we still create a 0-byte named file.
        hbdb_fs_put(stream->name, stream->buffer, stream->size);
    }
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
