#include "display.h"
#include "ports.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "../kernel/mem.h"
#include "../kernel/util.h"

unsigned char g_ConsoleColor = WHITE_ON_BLACK;
unsigned char g_ConsoleX=0, g_ConsoleY=0;
unsigned char g_CursorShow = 1;
unsigned char g_CursorChar = '_';

/* forward decls */
static int vsprintf_internal(char *buffer, const char *format, va_list args);

/* ================= VGA / SCREEN ================= */

int setpal() {
    port_byte_out(0x03C8, 0);
    port_byte_out(0x03C9, 0);
    port_byte_out(0x03C9, 0);
    port_byte_out(0x03C9, 0);

    port_byte_out(0x03C8, 15);
    port_byte_out(0x03C9, 255);
    port_byte_out(0x03C9, 255);
    port_byte_out(0x03C9, 255);

    for (int i = 1; i <= 14; i++) {
        port_byte_out(0x03C8, i);
        port_byte_out(0x03C9, 0);
        port_byte_out(0x03C9, 0);
        port_byte_out(0x03C9, i * 16);
    }

    g_ConsoleColor = WHITE_ON_BLACK;
    g_ConsoleX = 0;
    g_ConsoleY = 0;
    return 0;
}

void load_vga_font(const uint8_t *font, size_t height) {
    port_byte_out(0x3C4, 0x02);
    port_byte_out(0x3C5, 0x04);
    port_byte_out(0x3C4, 0x04);
    port_byte_out(0x3C5, 0x06);

    port_byte_out(0x3CE, 0x04);
    port_byte_out(0x3CF, 0x02);
    port_byte_out(0x3CE, 0x05);
    port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x06);
    port_byte_out(0x3CF, 0x00);

    uint8_t *plane2 = (uint8_t *)0xA0000;
    for (int ch = 0; ch < 256; ch++) {
        size_t i = 0;
        for (; i < height; i++) plane2[ch * 32 + i] = font[ch * height + i];
        for (; i < 32; i++)       plane2[ch * 32 + i] = 0x00;
    }

    port_byte_out(0x3C4, 0x02);
    port_byte_out(0x3C5, 0x03);
    port_byte_out(0x3C4, 0x04);
    port_byte_out(0x3C5, 0x02);

    port_byte_out(0x3CE, 0x04);
    port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x05);
    port_byte_out(0x3CF, 0x10);
    port_byte_out(0x3CE, 0x06);
    port_byte_out(0x3CF, 0x0E);
}

void save_vga_font(uint8_t *font, size_t height) {
    port_byte_out(0x3C4, 0x02);
    port_byte_out(0x3C5, 0x04);
    port_byte_out(0x3C4, 0x04);
    port_byte_out(0x3C5, 0x06);

    port_byte_out(0x3CE, 0x04);
    port_byte_out(0x3CF, 0x02);
    port_byte_out(0x3CE, 0x05);
    port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x06);
    port_byte_out(0x3CF, 0x00);

    uint8_t *plane2 = (uint8_t *)0xA0000;
    for (int ch = 0; ch < 256; ch++)
        for (size_t i = 0; i < height; i++)
            font[ch * height + i] = plane2[ch * 32 + i];

    port_byte_out(0x3C4, 0x02);
    port_byte_out(0x3C5, 0x03);
    port_byte_out(0x3C4, 0x04);
    port_byte_out(0x3C5, 0x02);

    port_byte_out(0x3CE, 0x04);
    port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x05);
    port_byte_out(0x3CF, 0x10);
    port_byte_out(0x3CE, 0x06);
    port_byte_out(0x3CF, 0x0E);
}

void set_color(unsigned char c) { g_ConsoleColor = c; }
unsigned char get_color(unsigned char c) { (void)c; return g_ConsoleColor; }

void set_cursor_char(unsigned char c) { g_CursorChar = c; }
unsigned char get_cursor_char() { return g_CursorChar; }

unsigned char isCursorVisible() { return g_CursorShow; }
void setCursorVisible(unsigned char visible) { g_CursorShow = visible; }

void set_cursor(int offset) {
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

int get_cursor() {
    port_byte_out(REG_SCREEN_CTRL, 14);
    int offset = port_byte_in(REG_SCREEN_DATA) << 8;
    port_byte_out(REG_SCREEN_CTRL, 15);
    offset += port_byte_in(REG_SCREEN_DATA);
    return offset * 2;
}

int get_offset(int col, int row) { return 2 * (row * MAX_COLS + col); }
int get_row_from_offset(int offset) { return offset / (2 * MAX_COLS); }
int move_offset_to_new_line(int offset) { return get_offset(0, get_row_from_offset(offset) + 1); }

void set_char_at_video_memory(char character, int offset) {
    uint8_t *vidmem = (uint8_t *)VIDEO_ADDRESS;
    vidmem[offset] = character;
    vidmem[offset + 1] = g_ConsoleColor;
}

int scroll_ln(int offset) {
    memory_copy(
        (uint8_t *)(get_offset(0, 1) + VIDEO_ADDRESS),
        (uint8_t *)(get_offset(0, 0) + VIDEO_ADDRESS),
        MAX_COLS * (MAX_ROWS - 1) * 2
    );
    for (int col = 0; col < MAX_COLS; col++)
        set_char_at_video_memory(' ', get_offset(col, MAX_ROWS - 1));
    return offset - 2 * MAX_COLS;
}

void print_string(char *string) {
    int offset = get_cursor();
    int i = 0;
    while (string[i] != 0) {
        if (offset >= MAX_ROWS * MAX_COLS * 2) offset = scroll_ln(offset);
        if (string[i] == '\n') offset = move_offset_to_new_line(offset);
        else { set_char_at_video_memory(string[i], offset); offset += 2; }
        i++;
    }
    set_cursor(offset);
}

void print_string_vertical(unsigned char x, unsigned char y, unsigned char* str) {
    for (int i = 0; i < (int)strlen((char*)str); i++)
        printChar(x, y + i, 0x0F, str[i]);
}

void print_nl() {
    int newOffset = move_offset_to_new_line(get_cursor());
    if (newOffset >= MAX_ROWS * MAX_COLS * 2) newOffset = scroll_ln(newOffset);
    set_cursor(newOffset);
}

void clear_screen() {
    int screen_size = MAX_COLS * MAX_ROWS;
    for (int i = 0; i < screen_size; ++i) set_char_at_video_memory(' ', i * 2);
    set_cursor(get_offset(0, 0));
}

void print_backspace() {
    int newCursor = get_cursor() - 2;
    set_char_at_video_memory(' ', newCursor);
    set_char_at_video_memory(' ', newCursor + 2);
    set_cursor(newCursor);
}

void printChar(unsigned char x, unsigned char y, unsigned char cl, char c) {
    unsigned char *videomemory = (unsigned char *)0xb8000 + (y << 7) + (y << 5) + (x << 1);
    *videomemory++ = c;
    *videomemory++ = cl;
}

void printString(unsigned char *str) {
    unsigned char *sp = str;
    static unsigned char x = 0, y = 0;
    while (*sp != '\0') {
        if (*sp == '\n') { y++; x = 0; sp++; continue; }
        printChar(x, y, 0x0F, *sp);
        x++; sp++;
    }
}

/* ================= helpers ================= */

void intToString(int value, char *buffer)
{
    if (value == 0) { buffer[0] = '0'; buffer[1] = '\0'; return; }

    int neg = 0;
    if (value < 0) { neg = 1; value = -value; }

    int numDigits = 0;
    int temp = value;
    while (temp != 0) { numDigits++; temp /= 10; }

    int i = numDigits - 1;
    while (value != 0) { buffer[i--] = '0' + (value % 10); value /= 10; }
    if (neg) buffer[++i] = '-';              /* place '-' at start */
    if (neg) { for (int j = numDigits; j >= 0; --j) buffer[j+1] = buffer[j]; buffer[0] = '-'; }
    buffer[numDigits + neg] = '\0';
}

void pointerToString(void *ptr, char *buffer)
{
    unsigned long value = (unsigned long)ptr;
    for (int i = 0; i < (int)(sizeof(void *) * 2); i++) {
        int digit = (value >> (sizeof(void *) * 8 - 4)) & 0xF;
        buffer[i] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value <<= 4;
    }
    buffer[sizeof(void *) * 2] = '\0';
}

void charToString(unsigned char value, char *buffer)
{
    int d1 = value >> 4;
    int d2 = value & 0x0F;
    buffer[0] = (d1 < 10) ? ('0' + d1) : ('A' + d1 - 10);
    buffer[1] = (d2 < 10) ? ('0' + d2) : ('A' + d2 - 10);
    buffer[2] = '\0';
}

void intToStringHex(unsigned int value, char *buffer, int numDigits)
{
    const char hexDigits[] = "0123456789ABCDEF";
    for (int i = numDigits - 1; i >= 0; i--) {
        int digit = (value >> (i * 4)) & 0xF;
        buffer[numDigits - 1 - i] = hexDigits[digit];
    }
    buffer[numDigits] = '\0';
}

int getHexDigitsCount(unsigned int value)
{
    if (value == 0) return 1;
    int count = 0;
    while (value != 0) { value >>= 4; count++; }
    return count;
}

void appendHexValue(unsigned int value, int digits, char* buffer)
{
    int index = digits - 1;
    while (index >= 0) {
        int hexDigit = value & 0xF;
        buffer[index] = (hexDigit < 10) ? (hexDigit + '0') : (hexDigit - 10 + 'A');
        value >>= 4;
        index--;
    }
}

void handleHexFormat(unsigned int value, int digits, char *buffer, int *charsWritten)
{
    int valueDigits = getHexDigitsCount(value);
    int leadingZeros = digits - valueDigits;
    if (leadingZeros < 0) leadingZeros = 0;

    for (int i = 0; i < leadingZeros; i++)
        buffer[(*charsWritten)++] = '0';

    appendHexValue(value, valueDigits, &buffer[*charsWritten]);
    (*charsWritten) += valueDigits;
}

void nibbleToChar(unsigned char nibble, char *buffer)
{
    *buffer = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
}

/* ================= printf family ================= */

/* REPLACE your vsprintf_internal with this one */
static int vsprintf_internal(char *buffer, const char *format, va_list args)
{
    int n = 0;

    while (*format) {
        if (*format != '%') { buffer[n++] = *format++; continue; }
        ++format; /* skip % */

        /* parse flags + width: %0NNd */
        int zero = 0, width = 0;
        if (*format == '0') { zero = 1; ++format; }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            ++format;
        }

        char spec = *format ? *format++ : '\0';

        switch (spec) {
        case '%':
            buffer[n++] = '%';
            break;

case 'f': {
    double v = va_arg(args, double);

    // default precision = 6
    int precision = 6;

    // check for ".N"
    if (*format == '.') {
        ++format;
        precision = 0;
        while (*format >= '0' && *format <= '9') {
            precision = precision * 10 + (*format - '0');
            ++format;
        }
    }

    // handle sign
    if (v < 0) {
        buffer[n++] = '-';
        v = -v;
    }

    // integer part
    unsigned long ipart = (unsigned long)v;
    double fpart = v - (double)ipart;

    // convert integer
    char tmp[32]; int i = 0;
    do { tmp[i++] = '0' + (ipart % 10); ipart /= 10; } while (ipart);
    while (i--) buffer[n++] = tmp[i];

    buffer[n++] = '.';

    // fractional part
    for (int p = 0; p < precision; p++) {
        fpart *= 10.0;
        int digit = (int)fpart;
        buffer[n++] = '0' + digit;
        fpart -= digit;
    }
    break;
}

        case 'c': {
            int c = va_arg(args, int);
            buffer[n++] = (char)c;
            break;
        }

        case 's': {
            const char *s = va_arg(args, const char*);
            if (!s) s = "(null)";
            int len = 0; for (const char *t = s; *t; ++t) ++len;
            int pad = width - len; while (pad-- > 0) buffer[n++] = zero ? '0' : ' ';
            while (*s) buffer[n++] = *s++;
            break;
        }

        case 'd': {
            int v = va_arg(args, int);
            unsigned int u = (v < 0) ? (unsigned int)(-v) : (unsigned int)v;

            char tmp[16]; int i = 0;
            do { tmp[i++] = (char)('0' + (u % 10)); u /= 10; } while (u);

            int sign = (v < 0);
            int pad = width - i - sign;
            if (sign) buffer[n++] = '-';
            while (pad-- > 0) buffer[n++] = zero ? '0' : ' ';
            while (i--) buffer[n++] = tmp[i];
            break;
        }

        case 'X': {
            unsigned int v = va_arg(args, unsigned int);
            char tmp[16]; int i = 0;
            do {
                unsigned int nib = v & 0xF;
                tmp[i++] = (char)(nib < 10 ? '0' + nib : 'A' + (nib - 10));
                v >>= 4;
            } while (v || i == 0); /* handle 0 */

            int pad = width - i;
            while (pad-- > 0) buffer[n++] = zero ? '0' : ' ';
            while (i--) buffer[n++] = tmp[i];
            break;
        }

        case 'p': {
            void *p = va_arg(args, void*);
            unsigned long val = (unsigned long)p;
            char tmp[2 * sizeof(void*)]; int i = 0;
            do {
                unsigned long nib = val & 0xF;
                tmp[i++] = (char)(nib < 10 ? '0' + nib : 'A' + (nib - 10));
                val >>= 4;
            } while (val || i == 0);

            /* optional width applies to whole 0x.... */
            int total = 2 + i;
            int pad = (width > total) ? (width - total) : 0;

            buffer[n++] = '0'; buffer[n++] = 'x';
            while (pad-- > 0) buffer[n++] = zero ? '0' : ' ';
            while (i--) buffer[n++] = tmp[i];
            break;
        }

        default:
            /* unknown spec: emit literally */
            if (spec) buffer[n++] = spec;
            break;
        }
    }

    buffer[n] = '\0';
    return n;
}

int vsprintf(char *buffer, const char *format, va_list args)
{
    return vsprintf_internal(buffer, format, args);
}

int sprintf(char *buffer, const char *format, ...)
{
    va_list args; va_start(args, format);
    int r = vsprintf_internal(buffer, format, args);
    va_end(args);
    return r;
}

void printf(const char *format, ...)
{
    va_list args;
    g_CursorShow = 0;
    va_start(args, format);
    char buffer[512];
    vsprintf_internal(buffer, format, args);
    va_end(args);
    print_string(buffer);
    g_CursorShow = 1;
}

/* ================= hex utils / dumps ================= */

void printHexByte(unsigned char c) {
    char buffer[3];                 /* FIX: war 1 */
    charToString(c, buffer);
    print_string(buffer);
    print_string(" ");
}

void printPointer2(void *ptr)
{
    char buffer[sizeof(void *) * 2 + 1];
    pointerToString(ptr, buffer);
    print_string(buffer);
}

void hexdump(void *ptr, size_t size)
{
    unsigned char *data = (unsigned char *)ptr;
    const int bytesPerLine = 16;

    for (size_t i = 0; i < size; i += bytesPerLine)
    {
        printPointer2((void *)((uintptr_t)ptr + i));
        print_string(": ");

        for (int j = 0; j < bytesPerLine; j++)
        {
            if (i + j < size) printHexByte(data[i + j]);
            else              print_string("   ");
        }

        print_string("  ");

        for (int j = 0; j < bytesPerLine; j++)
        {
            if (i + j < size) {
                unsigned char v = data[i + j];
                char c = (v >= 32 && v <= 126) ? (char)v : '.';
                char s[2] = { c, 0 };
                print_string(s);
            } else {
                print_string(" ");
            }
        }
        print_string("\n");
    }
}
