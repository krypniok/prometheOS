#pragma once

typedef void (*editor_save_cb_t)(const unsigned char* buf, int len);
typedef void (*editor_run_cb_t)(void);

void editor_set_save_callback(editor_save_cb_t cb);
void editor_set_run_callback(editor_run_cb_t cb);

unsigned char* editor_get_buffer(void);
int editor_buffer_capacity(void);
void editor_reset_buffer(void);
void editor_set_filename_hint(const char* name);

int editor_main(void);