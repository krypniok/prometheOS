#pragma once

#include <stdint.h>

void hidecursor(void);
void showcursor(void);
void clear_cursor(void);

void printframe(int x, int y, int w, int h, unsigned char color);
void printframe_caption(int x, int y, int w, int h, unsigned char color, unsigned char* caption);

void pf(void);
void dtmf(void);
void process_input(const char *input);
int bell(void);

// Selection UIs
int print_select_list_horizontal(void);
int print_select_list_vertical(void);

// Message box API
int message_box(int x, int y, int w, int h, unsigned char color,
                unsigned char* caption, unsigned char* message, unsigned char num_buttons);
void msgbox(void);

// Centered input prompt (Borland-style)
// Returns 1 if user confirmed with ENTER, 0 on ESC/cancel. Writes NUL-terminated result to out.
int ui_prompt_box(char* out, int outsz, const char* title, const char* label);
// Extended: allows providing initial content (prefill)
int ui_prompt_box_ex(char* out, int outsz, const char* title, const char* label, const char* initial);

// pl-style centered file prompt with OK/Cancel buttons
// Returns 1 on OK (out filled, trimmed and non-empty), 0 on cancel/ESC
int ui_file_prompt(char* out, int outsz, const char* title, const char* label, const char* initial);
