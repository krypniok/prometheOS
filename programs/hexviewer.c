#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../drivers/ports.h"
#include "../drivers/video.h"
#include "../stdlibs/string.h"
#include "../kernel/util.h"

int hexviewer(uint32_t address) {
    int sector = 0;
    char run = 1;
    unsigned char cx=0;
    unsigned char cy=0;
    unsigned char px=0;
    unsigned char py=0;
    void* cursor_address = (void*)address;
    int cursor;
    while(run) {
        unsigned char key = getkey();
        if (key == 0 || key == 0xE0) continue; 
        switch(key) {
            case SC_PAGEUP : sector--; break; 
            case SC_PAGEDOWN : sector++; break;
            case 72 : if(cy > 0) cy--; break; 
            case 80 : if(cy < 22) cy++; break;
            case 75 : if(cx > 0) cx--; break; 
            case 77 : if(cx < 15) cx++; break;
            case 1: run = 0; break;
            default: {
                if(key < 97) {
                unsigned char letter = char_from_key(key);
                key = letter;
                break;
                }
            }
        }

        clear_screen();
        draw_status_bar();
        cursor = get_cursor();
        set_cursor(0);
        cursor_address = ((void*)address+(368*sector))+(cy*16)+cx;
        printf("Cursor: %X, ", cursor_address);
        printf("Char: %c\n", key);
        set_cursor(cursor);

        hexdump(((void*)address+(368*sector)), 368);

        draw_status_bar();
        px = 10+(cx*3);
        py = 1+cy;
        set_cursor_xy(px, py);
    }
    set_color(FG_WHITE | BG_BLACK);
    clear_screen();
}
