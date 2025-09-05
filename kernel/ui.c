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

// ---- Selection lists (moved from stdlibs/textui.c) -----------------------
void draw_status_bar(void) {
    unsigned char old = get_color();
    set_color(FG_BLACK | BG_CYAN);
    set_cursor_xy(0, 0);
    for (int i = 0; i < 80; i++) printf(" ");
    set_color(old);
}

int print_select_list_horizontal() {
    unsigned char* list[2] = {"Ja", "Nein"};
    unsigned char* question = " Question ";
    int x = 1, y = 1, w=20, h=10;
    int selected = 0;
    unsigned char old_color = get_color();
    printframe_caption(x, y, w, h, FG_WHITE | BG_LIGHT_BLUE, question);
    // Trim whitespace in the result (both ends) before finishing
    /* no-op here; trimming does not apply in selection list */
    hidecursor();
    x += 3;
    y += h - 3;
    while (1) {
        set_cursor_xy(x, y);
        uint8_t scancode = getkey();
        if (scancode == SC_KEYPAD_4) { if(selected > 0) selected--; }
        else if (scancode == SC_KEYPAD_6) { if(selected < 1) selected++; }
        else if (scancode == SC_ESC) { set_color(old_color); showcursor(); return -1; }
        else if (scancode == SC_ENTER) { goto done; }
        console_begin_batch();
        for(int i=0; i<2; i++) {
            if(i == selected) { set_color(FG_BRIGHT_WHITE | BG_BLUE); printf("[ %s ]", list[i]); }
            else { set_color(FG_BRIGHT_WHITE | BG_LIGHT_BLUE); printf("[ %s ]", list[i]); }
            set_color(FG_BRIGHT_WHITE | BG_LIGHT_BLUE); if(i < 1) printf("  ");
        }
        printf("\n");
        console_end_batch();
        set_color(old_color);
    }
done:
    kernel_console_clear();
    set_color(old_color);
    printf("Selected: %d\n", selected);
    showcursor();
    return selected;
}

int print_select_list_vertical() {
    unsigned char* list[8] = {"Ja", "Nein", "Drei", "Vier", "F\x81nf", "Sechs", "Sieben", "Acht"};
    int listlen = sizeof(list) / sizeof(list[0]);
    unsigned char* question = " 12345678 ";
    int x = 1, y = 1, w=20, h=20, x1 = 1, y1 = 1;
    int selected = 0;
    unsigned char old_color = get_color();
    printframe_caption(x, y, w, h, FG_WHITE | BG_LIGHT_BLUE, question);
    hidecursor();
    x += 3; y += 3; y1 = y;
    while (1) {
        set_cursor_xy(x, y);
        uint8_t scancode = getkey();
        if (scancode == SC_KEYPAD_8) { if(selected > 0) selected--; }
        else if (scancode == SC_KEYPAD_2) { if(selected < listlen-1) selected++; }
        else if (scancode == SC_ESC) { return -1; }
        else if (scancode == SC_ENTER) { goto done; }
        console_begin_batch();
        for(int i=0; i<listlen; i++) {
            if(i == selected) { set_color(FG_WHITE | BG_BLUE); y1 = y+i; set_cursor_xy(x, y1); printf("%s", list[i]); }
            else { set_color(FG_WHITE | BG_BLACK); y1 = y+i; set_cursor_xy(x, y1); printf("%s", list[i]); }
            set_color(FG_BRIGHT_WHITE | BG_LIGHT_BLUE);
        }
        printf("\n");
        console_end_batch();
        set_color(old_color);
    }
done:
    kernel_console_clear();
    set_color(old_color);
    printf("Selected: %d\n", selected);
    showcursor();
    return selected;
}

// ---- Message box (moved from kernel_command) ------------------------------
int message_box(int x, int y, int w, int h, unsigned char color, unsigned char* caption, unsigned char* message, unsigned char num_buttons) {
    unsigned int old_color = get_color();
    unsigned char len = strlen(caption);
    unsigned char offset = (w - len) / 2;
    set_cursor_xy(x, y);
    printf("%c", 0xC9);
    for (int col = 0; col < (w-offset-len/2)-5; col++) { printf("%c", 0xCD); }
    printf("%s", caption);
    for (int col = 0; col < (w-offset-len/2)-5; col++) { printf("%c", 0xCD); }
    printf("%c\n", 0xBB);
    y++;
    for(int row=0; row<h-2; row++){
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
    set_color(color);
    set_cursor_xy(x + 2, y - h + 3);
    printf("%s", message);
    set_color(old_color);
    unsigned char* buttons[3] = {"Yes", "No", "Abort"};
    int selected = 0; unsigned char old_color2 = get_color(); hidecursor();
    while (1) {
        uint8_t scancode = getkey();
        if (scancode == SC_KEYPAD_4) { if (selected > 0) selected--; }
        else if (scancode == SC_KEYPAD_6) { if (selected < (int)num_buttons-1) selected++; }
        else if (scancode == SC_ESC) { set_color(old_color2); showcursor(); return -1; }
        else if (scancode == SC_ENTER) { break; }
        for(int i=0; i<(int)num_buttons; i++){
            if(i == selected) { set_color(FG_BRIGHT_WHITE | BG_LIGHT_BLUE); printf("%s", buttons[i]); }
            else { set_color(FG_BRIGHT_WHITE | BG_BLACK); printf("%s", buttons[i]); }
            set_color(FG_BRIGHT_WHITE | BG_BLACK); if(i < (int)num_buttons-1) printf(" | ");
        }
        printf("\n"); set_color(old_color2);
    }
    set_color(old_color2); showcursor(); return selected;
}

void msgbox(void){
    unsigned char* question = " question ? ";
    unsigned char* message = "This is 3 buttons.";
    int result = message_box(30, 7, 40, 10, FG_WHITE | BG_LIGHT_BLUE, question, message, 3);
    printf("Selected=%d\n", result);
}

// ---- Centered input prompt -------------------------------------------------
int ui_prompt_box_ex(char* out, int outsz, const char* title, const char* label, const char* initial){
    if (!out || outsz <= 0) return 0;
    out[0] = '\0';
    int w = 46, h = 7;
    int x = (80 - w) / 2;
    int y = (25 - h) / 2;
    int base_x = x + 2;
    int cy = y + 3;
    int maxlen = outsz - 1;
    int pos = 0;
    unsigned char old = get_color();

    console_begin_batch();
    printframe_caption(x, y, w, h, FG_WHITE | BG_LIGHT_BLUE, (unsigned char*)(title?title:(const char*)" input "));
    set_color(FG_WHITE | BG_BLACK);
    set_cursor_xy(base_x, y+2);
    printf("%s", label?label:"name: ");
    set_cursor_xy(base_x, cy);
    // prefill
    if (initial && initial[0]) {
        int n = strlen(initial);
        if (n > maxlen) n = maxlen;
        memcpy(out, initial, n);
        out[n] = '\0';
        pos = n;
        printf("%s", out);
    }
    console_end_batch();

    showcursor(); // allow caret to blink while typing
    while (1) {
        unsigned int sc = getkey();
        if ((sc & 0xFF) >= 0x80) continue; // ignore releases
        if (sc == SC_ESC) { out[0] = '\0'; break; }
        if (sc == SC_ENTER) { out[pos] = '\0'; break; }
        if (sc == SC_BACKSPACE) {
            if (pos > 0) {
                pos--; out[pos] = '\0';
                set_cursor_xy(base_x+pos, cy); printf("%c", ' ');
                set_cursor_xy(base_x+pos, cy);
            }
            continue;
        }
        unsigned char c = char_from_key(sc);
        if (c && pos < maxlen && (base_x + pos) < (x + w - 2)) {
            out[pos++] = c; out[pos] = '\0';
            printf("%c", c);
        }
    }
    // Trim whitespace in the result (both ends)
    int start = 0; while (out[start]==' '||out[start]=='\t') start++;
    if (start>0) { int i=0; do { out[i] = out[start+i]; } while (out[i++]!='\0'); }
    int L = (int)strlen(out); while (L>0 && (out[L-1]==' '||out[L-1]=='\t')) out[--L]='\0';
    hidecursor();
    set_color(old);
    return out[0] ? 1 : 0;
}

int ui_prompt_box(char* out, int outsz, const char* title, const char* label){
    return ui_prompt_box_ex(out, outsz, title, label, NULL);
}

// --- pl-style centered file prompt with buttons ----------------------------
int ui_file_prompt(char* out, int outsz, const char* title, const char* label, const char* initial)
{
    if (!out || outsz <= 0) return 0; out[0]='\0';
    int w = 50, h = 9;
    int x = (80 - w) / 2; if (x < 0) x = 0;
    int y = (25 - h) / 2; if (y < 1) y = 1;
    int maxlen = outsz - 1;
    unsigned char old = get_color();

    // Prefill
    if (initial && initial[0]) {
        int n = strlen(initial); if (n > maxlen) n = maxlen; memcpy(out, initial, n); out[n] = '\0';
    }

    // Draw frame and static parts
    printframe_caption(x, y, w, h, FG_WHITE | BG_LIGHT_BLUE, (unsigned char*)(title?title:(const char*)" file "));
    set_color(FG_WHITE | BG_BLACK);
    set_cursor_xy(x+2, y+2);
    printf("%s", label?label:"name: ");

    int base_x = x + 2 + (int)strlen(label?label:"name: ");
    int cy = y + 2;
    int pos = (int)strlen(out);

    // Buttons
    int sel = 0; // 0=OK, 1=Cancel
    auto void draw_buttons(){
        int cur = get_cursor();
        int by = y + h - 2;
        console_begin_batch();
        set_cursor_xy(x + w/2 - 10, by);
        if (sel==0) { set_color(FG_BRIGHT_WHITE|BG_BLUE); printf("[ OK ]"); }
        else         { set_color(FG_WHITE|BG_BLACK);       printf("[ OK ]"); }
        set_color(FG_WHITE|BG_BLACK); printf("  ");
        if (sel==1) { set_color(FG_BRIGHT_WHITE|BG_BLUE); printf("[ Cancel ]"); }
        else         { set_color(FG_WHITE|BG_BLACK);       printf("[ Cancel ]"); }
        console_end_batch();
        set_cursor(cur);
    };

    // Initial input print
    set_cursor_xy(base_x, cy);
    printf("%s", out);
    draw_buttons();
    showcursor();
    set_cursor_xy(base_x+pos, cy);

    while (1) {
        unsigned int sc = getkey(); if ((sc & 0xFF) >= 0x80) continue;
        if ((sc & 0xFF) == SC_ESC) { out[0]='\0'; break; }
        if ((sc & 0xFF) == SC_ENTER) {
            // OK when selected 0; if Cancel selected => cancel
            if (sel==1) { out[0]='\0'; break; }
            // Trim and accept if non-empty
            int s=0; while (out[s]==' '||out[s]=='\t') s++;
            if (s>0) { int i=0; do { out[i]=out[s+i]; } while (out[i++]!='\0'); }
            int L=(int)strlen(out); while (L>0 && (out[L-1]==' '||out[L-1]=='\t')) out[--L]='\0';
            if (out[0]) { break; }
            // keep prompting if empty
        }
        if (sc == SC_LEFT_ARROW)  { if (sel>0){ sel--; draw_buttons(); } set_cursor_xy(base_x+pos, cy); continue; }
        if (sc == SC_RIGHT_ARROW) { if (sel<1){ sel++; draw_buttons(); } set_cursor_xy(base_x+pos, cy); continue; }
        if ((sc & 0xFF) == SC_BACKSPACE) {
            if (pos>0){ pos--; out[pos]='\0'; set_cursor_xy(base_x+pos, cy); printf("%c", ' '); set_cursor_xy(base_x+pos, cy); }
            continue;
        }
        if ((sc & 0xFF) == SC_HOME) { pos=0; set_cursor_xy(base_x+pos, cy); continue; }
        if ((sc & 0xFF) == SC_END)  { pos=(int)strlen(out); set_cursor_xy(base_x+pos, cy); continue; }

        unsigned char ch = char_from_key(sc);
        if (ch && ch >= ' ' && pos < maxlen && (base_x+pos) < (x+w-2)) {
            out[pos++] = ch; out[pos] = '\0';
            printf("%c", ch);
            set_cursor_xy(base_x+pos, cy);
        }
    }

    hidecursor();
    set_color(old);
    return out[0] ? 1 : 0;
}
