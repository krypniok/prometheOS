#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../drivers/ports.h"
#include "../drivers/video.h"
#include "../stdlibs/string.h"
#include "../kernel/util.h"

#include "editor.h"

#define MAX_BUFFER_SIZE 1024
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

void draw_status_bar() {
    set_color(FG_BLACK | BG_CYAN);
    for(unsigned char c=0; c<80; c++) {
        printf(" ");
    }
}

void editor_exit() {
    set_color(FG_WHITE | BG_BLACK);
    clear_screen();
}

unsigned char *insert_character(unsigned char *text_buffer, unsigned char c) {
    int len = strlen(text_buffer);
    if (len < MAX_BUFFER_SIZE - 1) {
        text_buffer[len] = c;
        text_buffer[len + 1] = '\0'; // Nullterminator aktualisieren
    }
    return text_buffer;
}

void display_text_buffer(unsigned char *text_buffer, int num_lines) {
    set_color(FG_WHITE | BG_LIGHT_BLUE);
    int current_line = 0;
    for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
        if (text_buffer[i] != '\0') {
            if(text_buffer[i] != 0x0D) printf("%c", text_buffer[i]);
            if (text_buffer[i] == '\n') {
                current_line++;
            }
            if (current_line >= num_lines) {
                break; // Abbrechen, wenn die sichtbare Anzahl der Zeilen erreicht ist
            }
        }
    }
}

int count_lines(unsigned char *text_buffer) {
    int num_lines = 0;
    for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
        if (text_buffer[i] == '\n') {
            num_lines++;
        }
    }
    return num_lines + 1; // Zähle die letzte Zeile
}

int editor_main() {
    unsigned char* text_buffer = (unsigned char*)0x200000; //[MAX_BUFFER_SIZE] = {0};
    int num_lines = count_lines(text_buffer);
    int scroll_offset = 0;

    while (1) {
        clear_screen();
        draw_status_bar();
        display_text_buffer(text_buffer + scroll_offset, num_lines);
        unsigned int scancode = getkey();
        unsigned char letter = char_from_key(scancode);
        if (scancode == SC_ESC) {
            editor_exit();
            break;
        } else if (scancode == SC_F1) {
            text_buffer[0] = '\0';
            num_lines = 0;
        } else if (scancode == SC_ENTER) {
            insert_character(text_buffer, '\n');
            num_lines++;
        } else if (scancode == SC_BACKSPACE) {
            text_buffer[strlen(text_buffer) - 1] = '\0';
        } else if (scroll_offset > 0 && num_lines > SCREEN_HEIGHT) {
            if (scancode == SC_UP_ARROW) {
                beep(440, 500);
                scroll_offset--;
            }
        } else if (scancode == SC_DOWN_ARROW && num_lines > SCREEN_HEIGHT) {
            if (scroll_offset < num_lines - SCREEN_HEIGHT) {
                scroll_offset++;
                beep(440, 500);
            }
        } else if (scancode < 128) {
            insert_character(text_buffer, letter);
        }

        
    }
    return 0;
}
