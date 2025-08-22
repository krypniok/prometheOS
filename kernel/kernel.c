#include "../cpu/idt.h"
#include "../cpu/isr.h"
#include "../cpu/timer.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/ports.h"
#include "../drivers/video.h"
#include "../stdlibs/string.h"
#include "../cpu/jmpbuf.h"

#include "kernel.h"
#include "util.h"
#include "time.h"


// Funktion zum Verstecken des Cursors
void hidecursor() {
    // Index 0x0A entspricht dem Cursor-Form Control Register
    port_byte_out(0x3D4, 0x0A);
    unsigned char cursorControl = port_byte_in(0x3D5);

    // Bit 5 auf 1 setzen, um den Cursor zu verstecken
    cursorControl |= 0x20;

    // Neuen Wert an das CRTC Data Register senden
    port_byte_out(0x3D5, cursorControl);
}

// Funktion zum Anzeigen des Cursors
void showcursor() {
    // Index 0x0A entspricht dem Cursor-Form Control Register
    port_byte_out(0x3D4, 0x0A);
    unsigned char cursorControl = port_byte_in(0x3D5);

    // Bit 5 auf 0 setzen, um den Cursor anzuzeigen
    cursorControl &= 0xDF;

    // Neuen Wert an das CRTC Data Register senden
    port_byte_out(0x3D5, cursorControl);
}

void clear_cursor() {
    int newCursor = get_cursor();
    set_char_at_video_memory(' ', newCursor);
}

void printframe(int x, int y, int w, int h, unsigned char color) {
    unsigned int old_color = get_color();
    set_color(color);
    set_cursor_xy(x, y);
    printf("%c", 0xC9); // ecke links oben
    for (int col = 0; col < w; col++) { printf("%c", 0xCD); } // balken oben
    printf("%c\n", 0xBB); // ecke rechts oben
    y++;
    for(int row=0; row<h; row++) {
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
    set_color(old_color);
}

void printframe_caption(int x, int y, int w, int h, unsigned char color, unsigned char* caption) {
    unsigned int old_color = get_color();
    unsigned char len = strlen(caption);
    unsigned char offset = (w - len) / 2;

    set_color(color);
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
    set_color(old_color);
}

void pf() {
    printframe_caption(30, 7, 20, 10, FG_WHITE | BG_LIGHT_BLUE, " Question ");
  //  printframe(2, 2, 74, 23, FG_BRIGHT_WHITE | BG_BLUE);
}

void dtmf() {
    playDTMF("*31#0461#");    
}


void process_input(const char *input) {
    int len = strlen(input);
    int i = 0;
    char token[256]; // Annahme: Maximale Länge eines Tokens ist 255 Zeichen

    while (i < len) {
        int token_index = 0;

        // Token sammeln, bis \a oder das Ende des Strings erreicht ist
        while (i < len && input[i] != '\a') {
            token[token_index++] = input[i++];
        }

        // Null-Terminator hinzufügen
        token[token_index] = '\0';

        // Prüfen, ob das Token "freq:len" ist
        char *colon = strchr(token, ':');
        if (colon != NULL) {
            // Interpretieren als Frequenz und Länge
            int freq, len;
            if (sscanf(token, "%d:%d", &freq, &len) == 2) {
                printf("Bell: Frequency=%d ", freq);
                printf("Length=%d\n", len);
                beep(freq, len);
            }
        } else if (token_index > 0) { // Nur wenn Token nicht leer ist
            // Interpretieren als Text für eSpeak
            printf("eSpeak: %s\n", token);
            playDTMF(token);
        }

        i++; // Zum nächsten Zeichen gehen (Überspringen von \a)
    }
}

int bell() {
    const char *input = "\a0123456789\a\a440:500\a\a*31#9876543210#\a";
    process_input(input);
    return 0;
}


#define KERNEL_PROMPT_CHAR 0x10

//static char kernel_console_key_buffer[1024];
jmp_buf g_jmpKernelMain;

bool g_bKernelShouldStop = false;
bool g_bKernelInitialised = false;
int g_iKernelRevnum = REVISION_NUMBER;
unsigned char* g_strKernelRevdate = REVISION_DATE;


int kernel_console_program();
void loaddisk();

#include "../cpu/jmpbuf.h"
jmp_buf kernel_env;

#define REVISION_NAME "PrometheOS"

//kernel.c
void kernel_main() {
    setjmp(&kernel_env);
	g_jmpKernelMain =  kernel_env;
    set_color(WHITE_ON_BLACK);
    clear_screen();
    printf("start_kernel @ 0x%p\n", &kernel_main);
    debug_puts("start_kernel via port 0xe9\n");

	unsigned char str[80];
    sprintf(str, "%s 0.%d (%s)\n", REVISION_NAME, (void*)g_iKernelRevnum, (void*)g_strKernelRevdate);
    printf(str);

    if(! g_bKernelInitialised) {
        print_string("Installing interrupt service routines (ISRs).\n");
        isr_install();
        print_string("Enabling external interrupts.\n");
        asm volatile("sti");
        print_string("Initializing keyboard (IRQ 1).\n");
        init_keyboard();
        print_string("Initializing memory managment ¿.\n");
        init_memory();
        init_dynamic_mem();

        print_string("A20 Line was activated by the MBR.\n");
        //enable_a20_line();

        print_string("Initializing timer millisecond.\n");
        init_timer(1000);

        print_string("Initializing PS/2 mouse interface.\n");
        mouse_install();

        print_string("Initializing random number generator.\n");;
        init_random();

        print_string("Initializing FPU.\n");;
        fpu_init();

        print_string("Initializing Perfomance Counter.\n");;
		perf_init();

		


        g_bKernelInitialised = true;
    }

    kernel_console_key_buffer[0] = '\0';

	loaddisk();
    printf("\n%c ", KERNEL_PROMPT_CHAR);

    while(!g_bKernelShouldStop) {
        kernel_console_program();
    }

end_of_kernel:
    print_nl();
    printf("Wow, we should get here...\nExiting...\nStopping the CPU...\nescape espace...\n");
    asm volatile("hlt");
    printf("P.S. Why is this still working when the CPU is officially stopped (hlt) ?\n");
}

#define printlist print_select_list_horizontal



void kernel_console_execute_command(char *input) {
    int cursor = get_cursor();
    if (strcmp(input, "") == 0) { goto none; }

    CALL_FUNCTION(msgbox)
    CALL_FUNCTION_ALIAS(pl, printlist)
    CALL_FUNCTION_ALIAS(pv, print_select_list_vertical)
    CALL_FUNCTION(memtest)
    CALL_FUNCTION_ALIAS(cls, kernel_console_clear)
        CALL_FUNCTION_ALIAS(clear, kernel_console_clear)
            CALL_FUNCTION_ALIAS(clr, kernel_console_clear)
                CALL_FUNCTION_ALIAS(rst, kernel_console_clear)
                    CALL_FUNCTION_ALIAS(reset, kernel_console_clear)
    CALL_FUNCTION(killtimer)
    CALL_FUNCTION(bell)
    CALL_FUNCTION(snaketext)
    CALL_FUNCTION(ramdisk_test)
    CALL_FUNCTION(tinysql)
    CALL_FUNCTION(tinysql2)
    CALL_FUNCTION(restart)
    CALL_FUNCTION(main_c)
    
	CALL_FUNCTION_ALIAS(em, editor_main)

    CALL_FUNCTION_ALIAS(demo, bgademo)
    CALL_FUNCTION_ALIAS(sb16, sb16demo)

    CALL_FUNCTION(dtmf)

   // CALL_FUNCTION(printlogo)
   // CALL_FUNCTION(ll_main)
    CALL_FUNCTION(setpal)
    CALL_FUNCTION(snake_main)
    CALL_FUNCTION(random)
    CALL_FUNCTION(uptime)
    CALL_FUNCTION(hidecursor)
    CALL_FUNCTION(showcursor)
    CALL_FUNCTION(print_registers)
    CALL_FUNCTION(keycodes)
    CALL_FUNCTION(exit)
    CALL_FUNCTION(loaddisk) 
    CALL_FUNCTION(printascii)
    CALL_FUNCTION_WITH_ARG(cat)
    CALL_FUNCTION_WITH_ARG(hexviewer)
    CALL_FUNCTION_WITH_ARG(run)

    CALL_FUNCTION_WITH_2ARGS(hexdump)
    CALL_FUNCTION_WITH_2ARGS(memset)
    CALL_FUNCTION_WITH_3ARGS(memcpy)
    CALL_FUNCTION_WITH_3ARGS(searchb)

	CALL_FUNCTION_WITH_2ARGS(beep)

    CALL_FUNCTION_WITH_STR(printf)
    CALL_FUNCTION_WITH_2ARGS_AND_STR(searchs)

    else
    {
        printf("%s ", KERNEL_PROMPT_UNKNOWN_COMMAND, input);
        printf("%s\n", input);
    }
    printf("%c ", KERNEL_PROMPT_CHAR);
    return;
none:
    set_cursor(cursor - 156);
    return;
}

unsigned int g_isExtendedKey = 0;
unsigned int isExtendedKey(unsigned int key) {
    return key >> 8 == 0xE0;
}

// Console program one
int kernel_console_program() {
    while (1) {
        unsigned int key = getkey();
        unsigned int chr = char_from_key(key);
        
        if (key == SC_BACKSPACE) {
            if (backspace(kernel_console_key_buffer)) {
                print_backspace();
            }
        } else if (key == SC_ENTER) {
            clear_cursor();
            print_nl();
            kernel_console_execute_command(kernel_console_key_buffer);
            kernel_console_key_buffer[0] = '\0';
        } 
        else if (key == SC_F1 && is_key_pressed(SC_LEFT_CTRL)) {
            editor_main();
            printf("%c ", KERNEL_PROMPT_CHAR);
        }
        else {
            append(kernel_console_key_buffer, chr);
            char str[2] = {chr, '\0'};
            print_string(str);
        }

    }
}
