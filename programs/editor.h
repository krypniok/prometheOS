#pragma once

int editor_main();
// Optional save callback used by editor on F2
typedef void (*editor_save_cb_t)(unsigned char* buf, int len);
void editor_set_save_callback(editor_save_cb_t cb);
typedef void (*editor_run_cb_t)(void);
void editor_set_run_callback(editor_run_cb_t cb);
