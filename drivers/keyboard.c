#include <stdbool.h>
#include <stdint.h>

#include "keyboard.h"
#include "ports.h"
#include "../cpu/isr.h"
#include "display.h"
#include "../kernel/util.h"
#include "../kernel/kernel.h"

#include "hidden_cmd.h"

#define KEY_STATUS_MAX 128

static bool key_status[KEY_STATUS_MAX];
static bool extended_scancode = false;

// Define an array of key data
struct KeyData keyData[] = {
    {"ERROR", 0, 0, 0, SC_ERROR},
    {"Esc\0", 0, 0, 0, SC_ESC},
    {"KEY_1\0", '1', '!', 0xAD, SC_KEY_1},
    {"2", '2', '"', 0xFD, SC_KEY_2},
    {"3", '3', 0x15, 0xFC, SC_KEY_3},
    {"4", '4', '$', 0xAC, SC_KEY_4},
    {"5", '5', '%', 0xAB, SC_KEY_5},
    {"6", '6', '&', 0, SC_KEY_6},
    {"7", '7', '\'', '{', SC_KEY_7},
    {"8", '8', '(', '[', SC_KEY_8},
    {"9", '9', ')', ']', SC_KEY_9},
    {"0", '0', '=', '}', SC_KEY_0},
    {"Eszett", 0xE1, '?', '\\', SC_MINUS},
    {"EQUALS", 0x27, '`', 0xA8, SC_EQUALS},
    {"Backspace", '\b', 0, 0, SC_BACKSPACE},
    {"Tab", 0, 0, 0, SC_TAB},
    {"KEY_Q", 'q', 'Q', 0x40, SC_KEY_Q},
    {"W", 'w', 'W', 0xFB, SC_KEY_W},
    {"E", 'e', 'E', 0xEE, SC_KEY_E},
    {"R", 'r', 'R', 'p', SC_KEY_R},
    {"T", 't', 'T', 0xE9, SC_KEY_T},
    {"Z", 'z', 'Z', 'Z', SC_KEY_Z},
    {"U", 'u', 'U', 'U', SC_KEY_U},
    {"I", 'i', 'I', 'I', SC_KEY_I},
    {"O", 'o', 'O', 0xEA, SC_KEY_O},
    {"P", 'p', 'P', 0xE3, SC_KEY_P},
    {"KEY_UE", 0x81, 0x9A, 0, SC_LEFT_BRACKET},
    {"RIGHT_BRACKET", '+', '*', '~', SC_RIGHT_BRACKET},
    {"Enter", '\n', '\n', '\n', SC_ENTER},
    {"Lctrl", 0, 0, 0, SC_LEFT_CTRL},
    {"A", 'a', 'A', 0xE0, SC_KEY_A},
    {"S", 's', 'S', 0xE4, SC_KEY_S},
    {"D", 'd', 'D', 0xEB, SC_KEY_D},
    {"F", 'f', 'F', 0x9F, SC_KEY_F},
    {"G", 'g', 'G', 0xEC, SC_KEY_G},
    {"H", 'h', 'H', 0xEF, SC_KEY_H},
    {"J", 'j', 'J', 'J', SC_KEY_J},
    {"K", 'k', 'K', 'K', SC_KEY_K},
    {"L", 'l', 'L', 0x5E, SC_KEY_L},
    {"KEY_OE", 0x94, 0x99, 0, SC_SEMICOLON},
    {"KEY_AU", 0x84, 0x8E, 0, SC_APOSTROPHE},
    {"ACCENT", 0x5E, 0xF8, 0x27, SC_GRAVE_ACCENT},
    {"LShift", 0, 0, 0, SC_LEFT_SHIFT},
    {"BACKSLASH", '#', '\'', 0, SC_BACKSLASH},
    {"Y", 'y', 'Y', 0xE7, SC_KEY_Y},
    {"X", 'x', 'X', 'X', SC_KEY_X},
    {"C", 'c', 'C', 'C', SC_KEY_C},
    {"V", 'v', 'V', 'V', SC_KEY_V},
    {"B", 'b', 'B', 0xE1, SC_KEY_B},
    {"N", 'n', 'N', 'v', SC_KEY_N},
    {"KEY_M", 'm', 'M', 0xE6, SC_KEY_M},
    {"COMMA", ',', ';', 0, SC_COMMA},
    {"PERIOD", '.', ':', 0, SC_PERIOD},
    {"SLASH", '-', '_', 0, SC_SLASH},
    {"RShift", 0, 0, 0, SC_RIGHT_SHIFT},
    {"Keypad *", '*', '*', '*', SC_KEYPAD_ASTERISK}, 
    {"LAlt", 0, 0, 0, SC_LEFT_ALT},
    {"Spacebar", ' ', 0x03, ' ', SC_SPACEBAR},
    {"CapsLock", 0, 0, 0, SC_CAPS_LOCK},
    {"F1", 0, 0, 0, SC_F1},
    {"F2", 0, 0, 0, SC_F2},
    {"F3", 0, 0, 0, SC_F3},
    {"F4", 0, 0, 0, SC_F4},
    {"F5", 0, 0, 0, SC_F5},
    {"F6", 0, 0, 0, SC_F6},
    {"F7", 0, 0, 0, SC_F7},
    {"F8", 0, 0, 0, SC_F8},
    {"F9", 0, 0, 0, SC_F9},
    {"F10", 0, 0, 0, SC_F10},
    {"NumLock", 0, 0, 0, SC_NUM_LOCK},
    {"ScrollLock", 0, 0, 0, SC_SCROLL_LOCK},
    {"Keypad 7", '7', '7', '7', SC_KEYPAD_7},
    {"Keypad 8", '8', '8', '8', SC_KEYPAD_8},
    {"Keypad 9", '9', '9', '9', SC_KEYPAD_9},
    {"Keypad -", '-', '-', '-', SC_KEYPAD_MINUS},
    {"Keypad 4", '4', '4', '4', SC_KEYPAD_4},
    {"Keypad 5", '5', '5', '5', SC_KEYPAD_5},
    {"Keypad 6", '6', '6', '6', SC_KEYPAD_6},
    {"Keypad +", '+', '+', '+', SC_KEYPAD_PLUS},
    {"Keypad 1", '1', '1', '1', SC_KEYPAD_1},
    {"Keypad 2", '2', '2', '2', SC_KEYPAD_2},
    {"Keypad 3", '3', '3', '3', SC_KEYPAD_3},
    {"Keypad 0", '0', '0', '0', SC_KEYPAD_0},
    {"Keypad .", ',', ',', ',', SC_KEYPAD_PERIOD},
    {"AltSysReq", 0, 0, 0, SC_ALT_SYS_REQ},
    {"LTGT", '<', '>', 0xB3, 91},
    {"LTGT", '<', '>', 0xB3, 92},
    {"F11", 0, 0, 0, SC_F11},
    {"F12", 0, 0, 0, SC_F12},
    {"UP_ARROW", 0, 0, 0, SC_UP_ARROW},
    {"DOWN_ARROW", 0, 0, 0, SC_DOWN_ARROW},
    {"LEFT_ARROW", 0, 0, 0, SC_LEFT_ARROW},
    {"RIGHT_ARROW", 0, 0, 0, SC_RIGHT_ARROW},
    {"PAGEUP", 0, 0, 0, SC_PAGEUP},
    {"PAGEDOWN", 0, 0, 0, SC_PAGEDOWN},
    {"HOME", 0, 0, 0, SC_HOME},
    {"END", 0, 0, 0, SC_END},
    {"INSERT", 0, 0, 0, SC_INSERT},
    {"DELETE", 0, 0, 0, SC_DELETE},
};


// Funktion zum Lesen des Tastaturstatus
uint8_t read_keyboard_status() {
    return port_byte_in(0x64);
}

// Funktion zum Lesen des Tastaturdatenports
uint8_t read_keyboard_data() {
    return port_byte_in(0x60);
}

unsigned char char_from_key(unsigned int scancode);

static void keyboard_callback(registers_t *regs) {
    uint8_t scancode = port_byte_in(0x60);

    // Check for extended scancode prefix
    if (scancode == 0xE0) {
        extended_scancode = true;
        return;
    }

    // Handle key release event
    if (scancode >= 0x80) {
        scancode -= 0x80;
        key_status[scancode] = false;
        return;
    }

    // Handle key press event
    if (scancode < KEY_STATUS_MAX) {
        key_status[scancode] = true;

        // Check if the scancode is extended
        if (extended_scancode) {
            // Handle extended scancode here, if needed
            extended_scancode = false; // Reset extended scancode flag
        } else {
            // Handle regular scancode here
        }
    }

    uint8_t key = scancode;
    uint8_t chr = char_from_key(key);
    handle_invisible_keypress(chr);
}


void init_keyboard() {
    register_interrupt_handler(IRQ1, keyboard_callback);
}

bool is_key_pressed(unsigned int scancode) {
    if (scancode < KEY_STATUS_MAX) {
        return key_status[scancode];
    }
    return false;
}

unsigned int getkey() {
    sleep(10);
    while (!(read_keyboard_status() & 0x01)) { }
    uint8_t scancode = read_keyboard_data();
    return extended_scancode ? (0xE0 << 8 | scancode) : scancode;
}

unsigned int getkey_async() {
    sleep(10);
    uint8_t status = read_keyboard_status();
    if (!(status & 0x01) || (status & 0x20)) {
        return 0;
    }
    uint8_t scancode = read_keyboard_data();
    return extended_scancode ? (0xE0 << 8 | scancode) : scancode;
}

// Warten auf einen Tastendruck und Rückgabe des ASCII-Werts
unsigned char char_from_key(unsigned int scancode) {
        // Überprüfen, ob die Taste gedrückt wurde (Bit 0 ist gesetzt)
        if (scancode & 0x80) {
            // Taste wurde losgelassen, ignorieren und weiter warten
        } else {
            char letter = 0xA8;  // Standard: Fragezeichen

            // Scancode als Index verwenden, um den passenden Eintrag in keyData zu finden
            if (scancode < sizeof(keyData) / sizeof(keyData[0])) {
                if (is_key_pressed(SC_LEFT_ALT)) {
                    letter = keyData[scancode].ascii_alt;
                } else if (is_key_pressed(SC_LEFT_SHIFT) || is_key_pressed(SC_RIGHT_SHIFT)) {
                    letter = keyData[scancode].ascii_upper;
                } else {
                    letter = keyData[scancode].ascii;
                }
            }
            return letter;
        }
        return 0;
}

// Warten auf einen Tastendruck und Rückgabe des ASCII-Werts
unsigned char getch() {
    while (1) {
        uint8_t scancode = getkey(); // Du musst hier deine Funktion für Tastaturdaten verwenden

        // Überprüfen, ob die Taste gedrückt wurde (Bit 0 ist gesetzt)
        if (scancode & 0x80) {
            // Taste wurde losgelassen, ignorieren und weiter warten
        } else {
            char letter = 0xA8;  // Standard: Fragezeichen

            // Scancode als Index verwenden, um den passenden Eintrag in keyData zu finden
            if (scancode < sizeof(keyData) / sizeof(keyData[0])) {
                if (is_key_pressed(SC_LEFT_ALT)) {
                    letter = keyData[scancode].ascii_alt;
                } else if (is_key_pressed(SC_LEFT_SHIFT)) {
                    letter = keyData[scancode].ascii_upper;
                } else {
                    letter = keyData[scancode].ascii;
                }
            }
            return letter;
        }
    }
}

// Lesen einer Zeichenfolge (bis Enter) von der Tastatur
void gets(char *buffer, int buffer_size) {
    int index = 0;
    
    while (1) {
        char c = getch();
        printf("%c", c);
        
        if (c == '\n') {
            // Enter-Taste wurde gedrückt, beende die Zeichenfolgeneingabe
            buffer[index] = '\0';
            return;
        } else if (c == '\b' && index > 0) {
            // Backspace-Taste wurde gedrückt, lösche das letzte Zeichen im Puffer
            index--;
            buffer[index] = '\0';
        } else if (c >= ' ' && index < (buffer_size - 1)) {
            // Füge das Zeichen dem Puffer hinzu, wenn es druckbar ist und der Puffer nicht voll ist
            buffer[index] = c;
            index++;
            buffer[index] = '\0';
        }
    }
}
