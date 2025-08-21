#include "../drivers/keyboard.h"
#include "../drivers/display.h"

int print_select_list_horizontal() {
    unsigned char* list[2] = {"Ja", "Nein"};
    unsigned char* question = " Question ";
    int x = 1, y = 1, w=20, h=10;
    int selected = 0;
    unsigned char old_color = get_color();
    printframe_caption(x, y, w, h, FG_WHITE | BG_LIGHT_BLUE, question);
    hidecursor();
    x += 3;
    y += h - 3;
    while (1) {
        set_cursor_xy(x, y);
        uint8_t scancode = getkey();
        if (scancode == SC_KEYPAD_4) {
            if(selected > 0) selected--;
        }
        else if (scancode == SC_KEYPAD_6) {
            if(selected < 1) selected++;
        }
        else if (scancode == SC_ESC) {
            set_color(old_color);
            showcursor();
            return -1;
        }
        else if (scancode == SC_ENTER) {
            goto none;
        }
       
        for(int i=0; i<2; i++) {
            if(i == selected) {
                set_color(FG_BRIGHT_WHITE | BG_BLUE);
                printf("[ %s ]", list[i]);
            } else {
                set_color(FG_BRIGHT_WHITE | BG_LIGHT_BLUE);
                printf("[ %s ]", list[i]);
            }
            set_color(FG_BRIGHT_WHITE | BG_LIGHT_BLUE);
            if(i < 1) printf("  ");
        }
        printf("\n");
        set_color(old_color);        
    }
none:
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
    x += 3;
    y += 3;
    y1 = y;
    while (1) {
        set_cursor_xy(x, y);
        uint8_t scancode = getkey();
        if (scancode == SC_KEYPAD_8) {
            if(selected > 0) selected--;
        }
        else if (scancode == SC_KEYPAD_2) {
            if(selected < listlen-1) selected++;
        }
        else if (scancode == SC_ESC) {
            return -1;
        }
        else if (scancode == SC_ENTER) {
            goto none;
        }
        //clear_screen();
        for(int i=0; i<listlen; i++) {
            if(i == selected) {
                set_color(FG_WHITE | BG_BLUE);
                y1 = y+i;
                set_cursor_xy(x, y1);
                printf("%s", list[i]);
            } else {
                set_color(FG_WHITE | BG_BLACK);
                y1 = y+i;
                set_cursor_xy(x, y1);
                printf("%s", list[i]);
            }
           set_color(FG_BRIGHT_WHITE | BG_LIGHT_BLUE);
           }
        printf("\n");
        set_color(old_color); 
        }
none:
    kernel_console_clear();
    set_color(old_color);
    printf("Selected: %d\n", selected);
    showcursor();
    return selected;
}
