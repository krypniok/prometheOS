#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../stdlibs/string.h"
#include "../kernel/util.h"

// Parameter für den Generator
unsigned int seed = 1234;

// Funktion zur Initialisierung des Generators
void init_random() {
    seed = (unsigned int)GetTicks();
}

// Funktion zur Generierung einer Pseudozufallszahl im Bereich [0, RAND_MAX]
unsigned int pseudo_random() {
    init_random();
    seed = (seed * 1103515245 + 12345) % 2147483648; // 2^31
    return seed;
}

// Funktion zur Generierung einer Pseudozufallszahl im Bereich [min, max]
int rand_range(int min, int max) {
    return min + (pseudo_random() % (max - min + 1));
}


typedef struct {
    unsigned char x, y;
} snake_pos;

unsigned char snake_buffer[80 * 25 * 2];

int snake_score = 666;
snake_pos head = {40, 12};

void snake_print_sprite(unsigned char x, unsigned char y, unsigned char cl, char c)
{
    unsigned char *videomemory = (unsigned char *)&snake_buffer[((y * 160) + x * 2 ) ];
    *videomemory = c;
    videomemory++;
    *videomemory = cl;
    videomemory++;
}

void draw_statusbar() {
    set_color(FG_BLACK | BG_CYAN);
    for(unsigned char c=0; c<69; c++) {
        snake_print_sprite(c, 0, 0x0C, ' ');
    }
}

void snake_exit() {
    set_color(FG_WHITE | BG_BLACK);
    clear_screen();
}

void clear_buffer() {
    memset(snake_buffer, 0, 80*25*2);
}

void copy_to_video_memory(void* framebuffer) {
    uint8_t *vidmem = (uint8_t *) VIDEO_ADDRESS;
    memcpy(vidmem, framebuffer, 80*25*2);
}


int snake_main() {

    head.x = 40;
    head.y = 12;

    while (1) {
//        clear_screen();
        memset(VIDEO_ADDRESS, 0, 80*25*2);
        snake_print_sprite(head.x, head.y, 0x0F, 0x03);
        //draw_statusbar();

        if(is_key_pressed(SC_KEYPAD_8)) {
            if(head.y > 0) head.y--;
        }
        if(is_key_pressed(SC_KEYPAD_2)) {
            if(head.y < 24) head.y++;
        }
        if(is_key_pressed(SC_KEYPAD_4)) {
            if(head.x > 0) head.x--;
        }
        if(is_key_pressed(SC_KEYPAD_6)) {
            if(head.x < 79) head.x++;
        }
        if(is_key_pressed(SC_KEYPAD_5)) {
            snake_print_sprite(rand_range(0, 80), rand_range(0, 25), rand_range(0, 16), rand_range(0, 256));
        }
        else if(is_key_pressed(SC_ESC)) {
            snake_exit();
            return 0;
        }

        copy_to_video_memory(snake_buffer);
        sleep(50);
    }
}
