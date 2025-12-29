#pragma once

void msgbox(void);
int ui_prompt_box_ex(char* out, int outsz, const char* title, const char* label, const char* initial);
int ui_file_prompt(char* out, int outsz, const char* title, const char* label, const char* initial);

// Audio UI helpers
void playDTMF(const char* sequence);
int bell(void);

// Cursor & Frame helpers
void hidecursor(void);
void showcursor(void);
void printframe(int x, int y, int w, int h, unsigned char color);
void printframe_caption(int x, int y, int w, int h, unsigned char color, unsigned char* caption);

int print_select_list_horizontal(void);
int print_select_list_vertical(void);