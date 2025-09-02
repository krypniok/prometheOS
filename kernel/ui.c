#include "../drivers/ports.h"
#include "../drivers/display.h"
#include "../drivers/video.h"
#include "../drivers/keyboard.h"
#include "../stdlibs/string.h"
#include "../kernel/util.h"

#include "ui.h"

// Cursor helpers
void hidecursor() {
    port_byte_out(0x3D4, 0x0A);
    unsigned char cursorControl = port_byte_in(0x3D5);
    cursorControl |= 0x20; // set bit5
    port_byte_out(0x3D5, cursorControl);
}

void showcursor() {
    port_byte_out(0x3D4, 0x0A);
    unsigned char cursorControl = port_byte_in(0x3D5);
    cursorControl &= 0xDF; // clear bit5
    port_byte_out(0x3D5, cursorControl);
}

void clear_cursor() {
    int newCursor = get_cursor();
    set_char_at_video_memory(' ', newCursor);
}

// Frame helpers
void printframe(int x, int y, int w, int h, unsigned char color) {
    unsigned int old_color = get_color();
    console_begin_batch();
    set_color(color);
    set_cursor_xy(x, y);
    printf("%c", 0xC9);
    for (int col = 0; col < w; col++) { printf("%c", 0xCD); }
    printf("%c\n", 0xBB);
    y++;
    for (int row = 0; row < h; row++) {
        set_cursor_xy(x, y);
        printf("%c", 0xBA);
        for (int col = 0; col < w; col++) { printf("%c", ' '); }
        printf("%c\n", 0xBA);
        y++;
    }
    set_cursor_xy(x, y);
    printf("%c", 0xC8);
    for (int col = 0; col < w; col++) { printf("%c", 0xCD); }
    printf("%c\n", 0xBC);
    console_end_batch();
    set_color(old_color);
}

void printframe_caption(int x, int y, int w, int h, unsigned char color, unsigned char* caption) {
    unsigned int old_color = get_color();
    unsigned char len = strlen(caption);
    unsigned char offset = (w - len) / 2;
    console_begin_batch();
    set_color(color);
    set_cursor_xy(x, y);
    printf("%c", 0xC9);
    for (int col = 0; col < (w - offset - len / 2) - 5; col++) { printf("%c", 0xCD); }
    printf("%s", caption);
    for (int col = 0; col < (w - offset - len / 2) - 5; col++) { printf("%c", 0xCD); }
    printf("%c\n", 0xBB);
    y++;
    for (int row = 0; row < h - 2; row++) {
        set_cursor_xy(x, y);
        printf("%c", 0xBA);
        for (int col = 0; col < w; col++) { printf("%c", ' '); }
        printf("%c\n", 0xBA);
        y++;
    }
    set_cursor_xy(x, y);
    printf("%c", 0xC8);
    for (int col = 0; col < w; col++) { printf("%c", 0xCD); }
    printf("%c\n", 0xBC);
    console_end_batch();
    set_color(old_color);
}

void pf() {
    printframe_caption(30, 7, 20, 10, FG_WHITE | BG_LIGHT_BLUE, " Question ");
}

void dtmf() {
    playDTMF("*31#0461#"); // now non-blocking (background)
}

void process_input(const char *input) {
    int len = strlen(input);
    int i = 0;
    char token[256];

    while (i < len) {
        int token_index = 0;
        while (i < len && input[i] != '\a') {
            token[token_index++] = input[i++];
        }
        token[token_index] = '\0';

        char *colon = strchr(token, ':');
        if (colon != NULL) {
            int freq, dur;
            if (sscanf(token, "%d:%d", &freq, &dur) == 2) {
                printf("Bell: Frequency=%d ", freq);
                printf("Length=%d\n", dur);
                beep(freq, dur); // non-blocking
            }
        } else if (token_index > 0) {
            printf("eSpeak: %s\n", token);
            playDTMF(token); // non-blocking
        }
        i++;
    }
}

int bell() {
    const char *input = "\a0123456789\a\a440:500\a\a*31#9876543210#\a";
    process_input(input);
    return 0;
}
