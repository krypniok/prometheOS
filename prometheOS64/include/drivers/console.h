#ifndef CONSOLE_H
#define CONSOLE_H

// Globale Variablen (extern, damit kernel_main darauf zugreifen kann)
extern unsigned short text_buffer[80 * 25];
extern int term_col;
extern int term_row;
extern unsigned int term_color;

void term_clear();
void term_putc(char c);
void term_puts(const char *s);
void term_puthex(unsigned int n);
void term_putdec(unsigned int n);
void render_screen();
void restore_mouse_bg(int x, int y);

#endif