#pragma once

#include <stddef.h>

// Minimal helpers to use TinySQL as a simple KV (name -> blob) store

void hbdb_ensure_fs_table(void);
int hbdb_fs_get(const char* name, unsigned char** out_data, size_t* out_size);
int hbdb_fs_put(const char* name, const unsigned char* data, size_t size);
int hbdb_fs_remove(const char* name);

// List FS entries via callback (name, size)
void hbdb_fs_list(void (*cb)(const char* name, size_t size));

// Serialize/deserialize full DB to/from memory buffers
int hbdb_serialize(unsigned char** out_buf, size_t* out_len);
int hbdb_deserialize(const unsigned char* data, size_t len);

// Persist DB to/from disk image at given LBA (bytes count = 1 MiB)
int hbdb_save_image(unsigned int lba_start);
int hbdb_load_image(unsigned int lba_start);

// Autosave control (save on each fclose)
void hbdb_set_autosave(int enable);
void hbdb_set_image_lba(unsigned int lba);
void hbdb_autosave_maybe(void);

// FS helpers
int hbdb_fs_rows(void); // -1 if FS table missing

// Limits
void hbdb_set_max_bytes(unsigned int max_bytes); // default 8 MiB
unsigned int hbdb_get_max_bytes(void);

// Calculate current serialized sizes (without writing):
// payload bytes = size of serialized tables; total bytes = 8-byte header + payload, padded to 512
unsigned int hbdb_calc_bytes_payload(void);
unsigned int hbdb_calc_bytes_total(void);
