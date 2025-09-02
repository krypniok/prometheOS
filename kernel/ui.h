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

