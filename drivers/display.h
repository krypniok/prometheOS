#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../cpu/isr.h"

#define VIDEO_ADDRESS       0xB8000
#define MAX_ROWS            25
#define MAX_COLS            80

#define FG_BLACK            0
#define FG_BLUE             1
#define FG_GREEN            2
#define FG_CYAN             3
#define FG_RED              4
#define FG_MAGENTA          5
#define FG_YELLOW           6
#define FG_WHITE            7
#define FG_GRAY             8
#define FG_LIGHT_BLUE       9
#define FG_LIGHT_GREEN      10
#define FG_LIGHT_CYAN       11
#define FG_LIGHT_RED        12
#define FG_LIGHT_MAGENTA    13
#define FG_LIGHT_YELLOW     14
#define FG_BRIGHT_WHITE     15

#define BG_BLACK            0
#define BG_BLUE             16
#define BG_GREEN            32
#define BG_CYAN             48
#define BG_RED              64
#define BG_MAGENTA          80
#define BG_YELLOW           96
#define BG_WHITE            112
#define BG_GRAY             128
#define BG_LIGHT_BLUE       144
#define BG_LIGHT_GREEN      160
#define BG_LIGHT_CYAN       176
#define BG_LIGHT_RED        192
#define BG_LIGHT_MAGENTA    208
#define BG_LIGHT_YELLOW     224
#define BG_BRIGHT_WHITE     240

#define WHITE_ON_BLACK FG_BRIGHT_WHITE | BG_BLACK

#define set_cursor_xy(x, y) set_cursor((y*160)+x*2)

/* Screen i/o ports */
#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

/* Public kernel API */
void print_string(char *string);
void print_string_vertical(unsigned char x, unsigned char y, unsigned char* str);

void print_nl();

void clear_screen();

int scroll_ln(int offset);

void print_backspace();

void set_cursor(int offset);
int get_cursor();

void set_color(unsigned char c);
int get_cursor();

void printChar(unsigned char x, unsigned char y, unsigned char cl, char c);

void printString(unsigned char *str);
void intToString(int value, char *buffer);
//void intToStringHex(int value, char *buffer, int numDigits);

void charToString(unsigned char value, char *buffer);

void pointerToString(void *ptr, char *buffer);

int sprintf( char *buffer, const char *format, ...);
void printf(const char *format, ...);

void hexdump(void *ptr, size_t size);


void printHexByte(unsigned char c);

void print_registers();

int setpal();
int get_offset(int col, int row);

void load_vga_font(const uint8_t *font, size_t height);
void save_vga_font(uint8_t *font, size_t height);
