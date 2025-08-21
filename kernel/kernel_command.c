#include <stddef.h>

#include "kernel.h"
#include "time.h"

#include "../drivers/keyboard.h"
#include "../drivers/display.h"
#include "../cpu/jmpbuf.h"

extern bool g_bKernelShouldStop;
extern jmp_buf g_jmpKernelMain;

void searchb(uint32_t address, uint32_t size, uint32_t byte) {
        printf("Searching %d\n", byte);
        void* result = (void*)search_byte((void*)address, size, byte);
        if (result != NULL) {
            char resultAddressStr[20];
            sprintf(resultAddressStr, "%p", &result);
            print_string("Byte found at address: ");
            print_string(resultAddressStr);
            print_string("\n");
        } else {
            print_string("Byte not found.\n");
        }
}

void searchs(uint32_t address, uint32_t size, unsigned char* string) {
        printf("Searching %s\n", &string);
        void* result = (void*)search_string((void*)address, size, string);
        if (result != NULL) {
            char resultAddressStr[20];
            sprintf(resultAddressStr, "%p", &result);
            print_string("String found at address: ");
            print_string(resultAddressStr);
            print_string("\n");
        } else {
            print_string("String not found.\n");
        }
}


void formatTimestamp(unsigned int timestamp_ms) {
    unsigned int ms_per_day = 86400000; // Millisekunden pro Tag
    unsigned int ms_per_hour = 3600000; // Millisekunden pro Stunde
    unsigned int ms_per_minute = 60000; // Millisekunden pro Minute
    unsigned int ms_per_second = 1000;  // Millisekunden pro Sekunde

    unsigned int days = timestamp_ms / ms_per_day;
    timestamp_ms %= ms_per_day;

    unsigned int hours = timestamp_ms / ms_per_hour;
    timestamp_ms %= ms_per_hour;

    unsigned int minutes = timestamp_ms / ms_per_minute;
    timestamp_ms %= ms_per_minute;

    unsigned int seconds = timestamp_ms / ms_per_second;
    unsigned int milliseconds = timestamp_ms % ms_per_second;

    // Ausgabe der Längenzeit
    printf("%d Tage\n", days);
    printf("%d Stunden\n", hours);
    printf("%d Minuten\n", minutes);
    printf("%d Sekunden\n", seconds);
    printf("%d Millisekunden\n", milliseconds);
}

unsigned char strUptime[80];
void formatTimestampHHMMSS(unsigned int timestamp_ms) {
    unsigned int ms_per_day = 86400000; // Millisekunden pro Tag
    unsigned int ms_per_hour = 3600000; // Millisekunden pro Stunde
    unsigned int ms_per_minute = 60000; // Millisekunden pro Minute
    unsigned int ms_per_second = 1000;  // Millisekunden pro Sekunde

    unsigned int days = timestamp_ms / ms_per_day;
    timestamp_ms %= ms_per_day;

    unsigned int hours = timestamp_ms / ms_per_hour;
    timestamp_ms %= ms_per_hour;

    unsigned int minutes = timestamp_ms / ms_per_minute;
    timestamp_ms %= ms_per_minute;

    unsigned int seconds = timestamp_ms / ms_per_second;
    unsigned int milliseconds = timestamp_ms % ms_per_second;

    int revnum = REVISION_NUMBER;
    unsigned char* revdate = REVISION_DATE;
    sprintf(strUptime, "Uptime: %02d:%02d:%02d\n", hours, minutes, seconds);

    print_string(strUptime);
}




void uptime() {
    formatTimestampHHMMSS(GetTicks());
}

void exit() {
    g_bKernelShouldStop = true;
}

void run(void* address) {
    FunctionPointer funcPtr = (FunctionPointer)address;
    funcPtr();
}

void random() {
    for (int i = 0; i < 10; i++) {
        printf("%d\n", rand_range(1, 100)); // Beispiel: Zahlen zwischen 1 und 100
    }
}  


void killtimer() {
    remove_sub_timer(0);
}

int restart() {
    longjmp(&g_jmpKernelMain, 0);
    return 0;
}

void loaddisk() {
    printf("Loading Disk (1.44MB) to 0x200000\n");
    for(unsigned int i=0; i<2880; i++) {
        read_from_disk(i, (void*)0x200000+(512*i), 512);
    }
}

void cat(const char *addr) {
    while (*addr != '\0') {
        printf("%c", *addr);
        addr++;
    }
    printf("\n");
}

int keycodes() {
    while (1) {
        uint8_t scancode = getkey();
        if (scancode < 128) {
            printf("%s\n", keyData[scancode].name);
            if (scancode == SC_ESC) {
                return 0;
            }
        }
    }
    sleep(33);
}

void kernel_console_clear() {
        clear_screen();
        set_cursor(0);
}

    int message_box(int x, int y, int w, int h, unsigned char color, unsigned char* caption, unsigned char* message, unsigned char num_buttons) {
        unsigned int old_color = get_color();
        unsigned char len = strlen(caption);
        unsigned char offset = (w - len) / 2;
        set_cursor_xy(x, y);
        printf("%c", 0xC9); // ecke links oben
     
        for (int col = 0; col < (w-offset-len/2)-5; col++) { printf("%c", 0xCD); } // balken oben
        printf("%s", caption); // balken oben
        for (int col = 0; col < (w-offset-len/2)-5; col++) { printf("%c", 0xCD); } // balken oben

        printf("%c\n", 0xBB); // ecke rechts oben
        y++;
        for(int row=0; row<h-2; row++) {
            set_cursor_xy(x, y);
            printf("%c", 0xBA); // balken links
            for (int col = 0; col < w; col++) { printf("%c", ' '); } // leerzeichen
            printf("%c\n", 0xBA); // balken rechts
            y++;
        }
        set_cursor_xy(x, y);
        printf("%c", 0xC8); // ecke links unten
        for (int col = 0; col < w; col++) { printf("%c", 0xCD); } // balken unten
        printf("%c\n", 0xBC); // ecke rechts unten
        set_color(color);
        set_cursor_xy(x + 2, y - h + 3);
        printf("%s", message);
        set_color(old_color);
        unsigned char* buttons[3] = {"Yes", "No", "Abort"};
        int selected = 0;
        unsigned char old_color2 = get_color();
        hidecursor();
        while (1) {
            uint8_t scancode = getkey();
            if (scancode == SC_KEYPAD_4) {
                if(selected > 0) selected--;
            }
            else if (scancode == SC_KEYPAD_6) {
                if(selected < num_buttons-1) selected++;
            }
            else if (scancode == SC_ESC) {
                set_color(old_color2);
                showcursor();
                return -1;
            }
            else if (scancode == SC_ENTER) {
                goto none;
            }
            //clear_screen();
            for(int i=0; i<num_buttons; i++) {
                if(i == selected) {
                    set_color(FG_BRIGHT_WHITE | BG_LIGHT_BLUE);
                    printf("%s", buttons[i]);
                } else {
                    set_color(FG_BRIGHT_WHITE | BG_BLACK);
                    printf("%s", buttons[i]);
                }
                set_color(FG_BRIGHT_WHITE | BG_BLACK);
                if(i < num_buttons-1) printf(" | ");
            }
            printf("\n");
            set_color(old_color2);        
        }
    none:
        set_color(old_color2);
        printf("Selected: %s\n", buttons[selected]);
        showcursor();
        return selected;
    }

    void msgbox() {
        unsigned char* question = " question ? ";
        unsigned char* message = "This is 3 buttons.";
        int result = message_box(30, 7, 40, 10, FG_WHITE | BG_LIGHT_BLUE, question, message, 3);
        if (result == 0) {
            printf("Yes was selected.\n");
        } else if (result == 1) {
            printf("No was selected.\n");
        } else if (result == 2) {
            printf("Abort was selected.\n");
        } else {
            printf("Message box was cancelled.\n");
        }
    }
    