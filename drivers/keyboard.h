#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#define SC_ERROR             0
#define SC_ESC               1
#define SC_KEY_1             2
#define SC_KEY_2             3
#define SC_KEY_3             4
#define SC_KEY_4             5
#define SC_KEY_5             6
#define SC_KEY_6             7
#define SC_KEY_7             8
#define SC_KEY_8             9
#define SC_KEY_9            10
#define SC_KEY_0            11
#define SC_MINUS            12
#define SC_EQUALS           13
#define SC_BACKSPACE        14
#define SC_TAB              15
#define SC_KEY_Q            16
#define SC_KEY_W            17
#define SC_KEY_E            18
#define SC_KEY_R            19
#define SC_KEY_T            20
#define SC_KEY_Z            21
#define SC_KEY_U            22
#define SC_KEY_I            23
#define SC_KEY_O            24
#define SC_KEY_P            25
#define SC_LEFT_BRACKET     26
#define SC_RIGHT_BRACKET    27
#define SC_ENTER            28
#define SC_LEFT_CTRL        29
#define SC_KEY_A            30
#define SC_KEY_S            31
#define SC_KEY_D            32
#define SC_KEY_F            33
#define SC_KEY_G            34
#define SC_KEY_H            35
#define SC_KEY_J            36
#define SC_KEY_K            37
#define SC_KEY_L            38
#define SC_SEMICOLON        39
#define SC_APOSTROPHE       40
#define SC_GRAVE_ACCENT     41
#define SC_LEFT_SHIFT       42
#define SC_BACKSLASH        43
#define SC_KEY_Y            44
#define SC_KEY_X            45
#define SC_KEY_C            46
#define SC_KEY_V            47
#define SC_KEY_B            48
#define SC_KEY_N            49
#define SC_KEY_M            50
#define SC_COMMA            51
#define SC_PERIOD           52
#define SC_SLASH            53
#define SC_RIGHT_SHIFT      54
#define SC_KEYPAD_ASTERISK  55
#define SC_LEFT_ALT         56
#define SC_SPACEBAR         57
#define SC_CAPS_LOCK        58
#define SC_F1               59
#define SC_F2               60
#define SC_F3               61
#define SC_F4               62
#define SC_F5               63
#define SC_F6               64
#define SC_F7               65
#define SC_F8               66
#define SC_F9               67
#define SC_F10              68
#define SC_NUM_LOCK         69
#define SC_SCROLL_LOCK      70
#define SC_KEYPAD_7         71
#define SC_KEYPAD_8         72
#define SC_KEYPAD_9         73
#define SC_KEYPAD_MINUS     74
#define SC_KEYPAD_4         75
#define SC_KEYPAD_5         76
#define SC_KEYPAD_6         77
#define SC_KEYPAD_PLUS      78
#define SC_KEYPAD_1         79
#define SC_KEYPAD_2         80
#define SC_KEYPAD_3         81
#define SC_KEYPAD_0         82
#define SC_KEYPAD_PERIOD    83
#define SC_ALT_SYS_REQ      84
#define SC_F11              85
#define SC_F12              86

/*
#define SC_UP_ARROW         87
#define SC_DOWN_ARROW       88
#define SC_LEFT_ARROW       89
#define SC_RIGHT_ARROW      90
*/

#define SC_PAGEUP           0x49
#define SC_PAGEDOWN         0x51
#define SC_HOME             0x47
#define SC_END              0x4F
#define SC_INSERT           0x52
#define SC_DELETE           0x53

#define SC_UP_ARROW         0xE048
#define SC_DOWN_ARROW       0xE050
#define SC_LEFT_ARROW       0xE04B
#define SC_RIGHT_ARROW      0xE04D

#define SC_F11_EXT          0xE05A
#define SC_F12_EXT          0xE05B

#define SC_RIGHT_CTRL       0xE01D
#define SC_RIGHT_ALT        0xE038

#define SC_PRINT_SCREEN     0xE037
#define SC_PAUSE            0xE045

// Define a struct to hold key data
struct KeyData {
    const unsigned char* name;
    unsigned char ascii;
    unsigned char ascii_upper;
    unsigned char ascii_alt;
    unsigned int scancode;
};


// Funktion zum Lesen des Tastaturstatus
uint8_t read_keyboard_status();
// Funktion zum Lesen des Tastaturdatenports
uint8_t read_keyboard_data();

// Define an array of key data
extern struct KeyData keyData[];

void init_keyboard();
bool is_key_pressed(unsigned int scancode);
unsigned int getkey();
unsigned int getkey_async();

#endif
