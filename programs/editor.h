#pragma once

int editor_main();
// Optional save callback used by editor on F2
typedef void (*editor_save_cb_t)(unsigned char* buf, int len);
void editor_set_save_callback(editor_save_cb_t cb);
typedef void (*editor_run_cb_t)(void);
void editor_set_run_callback(editor_run_cb_t cb);

// Shared working buffer helpers (1 MiB text/hex workspace)
unsigned char* editor_get_buffer(void);
int editor_buffer_capacity(void);
void editor_reset_buffer(void);

// Provide filename hint (used for status bar + save dialog defaults)
void editor_set_filename_hint(const char* name);
