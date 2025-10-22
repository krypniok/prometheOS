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
// Track modifier states explicitly to distinguish left/right variants
static bool mod_lshift=false, mod_rshift=false;
static bool mod_lctrl=false,  mod_rctrl=false;
static bool mod_lalt=false,   mod_ralt=false;
static bool capslock_on=false;

// Minimal DE AltGr layer (CP1252-ish codes where applicable)
// Index by base scancode (non-extended)
static const unsigned char de_altgr[128] = {
    [SC_KEY_Q] = '@',      // AltGr+Q => @
    [SC_KEY_E] = 0x80,     // AltGr+E => € (CP1252 0x80)
    [SC_KEY_M] = 0xB5,     // AltGr+M => µ
    [SC_KEY_7] = '{',      // AltGr+7 => {
    [SC_KEY_8] = '[',      // AltGr+8 => [
    [SC_KEY_9] = ']',      // AltGr+9 => ]
    [SC_KEY_0] = '}',      // AltGr+0 => }
    [SC_MINUS] = '\\',    // AltGr+ß => \
    // For '|' on DE key <>, map via keyData ascii_alt if available (scancode 0x56 not explicit here)
};

// Ring buffer for scancodes captured by the interrupt handler
#define KEY_BUFFER_SIZE 128
static unsigned int key_buffer[KEY_BUFFER_SIZE];
static volatile unsigned int key_buffer_head = 0;
static volatile unsigned int key_buffer_tail = 0;

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

    uint16_t final_code = extended_scancode ? (0xE0 << 8 | scancode) : scancode;

    if (scancode >= 0x80) {
        uint8_t base = scancode - 0x80;
        if (base < KEY_STATUS_MAX) {
            key_status[base] = false;
        }
        // Update modifier state on release
        if (extended_scancode) {
            if (base == 0x38) mod_ralt = false;      // RAlt (E0 38)
            if (base == 0x1D) mod_rctrl = false;     // RCtrl (E0 1D)
        } else {
            if (base == 0x2A || base == 0x36) { // LShift(2A) or RShift(36)
                if (base == 0x2A) mod_lshift = false; else mod_rshift = false;
            }
            if (base == 0x1D) mod_lctrl = false;     // LCtrl
            if (base == 0x38) mod_lalt  = false;     // LAlt
        }
        // store release scancode
        key_buffer[key_buffer_head] = final_code;
        key_buffer_head = (key_buffer_head + 1) % KEY_BUFFER_SIZE;
        extended_scancode = false;
        return;
    }

    if (scancode < KEY_STATUS_MAX) {
        key_status[scancode] = true;
    }

    if (!extended_scancode) {
        bool alt_combo   = (mod_lalt  || mod_ralt);
        bool shift_combo = (mod_lshift || mod_rshift);
        bool ctrl_combo  = (mod_lctrl || mod_rctrl);
        if ((alt_combo && shift_combo) || (ctrl_combo && shift_combo) || (ctrl_combo && alt_combo)) {
            int target = -1;
            if (scancode >= SC_F1 && scancode <= SC_F10) {
                target = (int)(scancode - SC_F1);
            } else if (scancode == SC_F11) {
                target = 10;
            } else if (scancode == SC_F12) {
                target = 11;
            }
            if (target >= 0) {
                kernel_request_vt_switch(target);
                extended_scancode = false;
                return;
            }
        }
    }

    // Update modifier state on press
    if (extended_scancode) {
        if (scancode == 0x38) mod_ralt = true;      // RAlt
        if (scancode == 0x1D) mod_rctrl = true;     // RCtrl
    } else {
        if (scancode == 0x2A || scancode == 0x36) { // LShift or RShift
            if (scancode == 0x2A) mod_lshift = true; else mod_rshift = true;
        }
        if (scancode == 0x1D) mod_lctrl = true;     // LCtrl
        if (scancode == 0x38) mod_lalt  = true;     // LAlt
        if (scancode == SC_CAPS_LOCK) capslock_on = !capslock_on; // toggle
    }

    // store press scancode
    key_buffer[key_buffer_head] = final_code;
    key_buffer_head = (key_buffer_head + 1) % KEY_BUFFER_SIZE;
    extended_scancode = false;

    uint8_t key = scancode;
    uint8_t chr = char_from_key(key);
    handle_invisible_keypress(chr);
}


void init_keyboard() {
    register_interrupt_handler(IRQ1, keyboard_callback);
}

bool is_key_pressed(unsigned int scancode) {
    // Handle extended right-side modifiers
    if (scancode == SC_RIGHT_CTRL) return mod_rctrl;
    if (scancode == SC_RIGHT_ALT)  return mod_ralt;
    if (scancode == 0x36 /*RShift base*/ || scancode == SC_RIGHT_SHIFT) return mod_rshift;
    if (scancode < KEY_STATUS_MAX) return key_status[scancode];
    return false;
}

const char* scancode_name(unsigned int code){
    static char buf[16];
    if ((code & 0xFF00) == 0xE000){
        unsigned int base = code & 0xFF;
        switch (base){
            case 0x1D: return "RCtrl";  // E0 1D
            case 0x38: return "RAlt";   // E0 38 (AltGr)
            case 0x48: return "UP";
            case 0x50: return "DOWN";
            case 0x4B: return "LEFT";
            case 0x4D: return "RIGHT";
            default:
                // Generic E0-xx
                buf[0]='E'; buf[1]='0'; buf[2]='-';
                const char hex[]="0123456789ABCDEF";
                buf[3]=hex[(base>>4)&0xF]; buf[4]=hex[base&0xF]; buf[5]='\0';
                return buf;
        }
    } else {
        unsigned int b = code & 0xFF;
        if (b < sizeof(keyData)/sizeof(keyData[0])) return (const char*)keyData[b].name;
    }
    return "?";
}

static unsigned int pop_key() {
    unsigned int scancode = key_buffer[key_buffer_tail];
    key_buffer_tail = (key_buffer_tail + 1) % KEY_BUFFER_SIZE;
    return scancode;
}

unsigned int getkey() {
    while (key_buffer_head == key_buffer_tail) {
        sleep(10);
    }
    return pop_key();
}

unsigned int getkey_async() {
    if (key_buffer_head == key_buffer_tail) {
        return 0;
    }
    return pop_key();
}

// Warten auf einen Tastendruck und Rückgabe des ASCII-Werts
unsigned char char_from_key(unsigned int scancode) {
        // Überprüfen, ob die Taste gedrückt wurde (Bit 0 ist gesetzt)
        if (scancode & 0x80) {
            // Taste wurde losgelassen, ignorieren und weiter warten
        } else {
            // Do not emit characters for pure modifier keys
            if (scancode == SC_LEFT_SHIFT || scancode == 0x36 /*RShift base*/ ||
                scancode == SC_LEFT_CTRL  || scancode == 0x1D /*LCtrl base*/  ||
                scancode == SC_LEFT_ALT   || scancode == 0x38 /*LAlt base*/ ) {
                return 0;
            }
            char letter = 0;  // Default: no visible char

            // Scancode als Index verwenden, um den passenden Eintrag in keyData zu finden
            if (scancode < sizeof(keyData) / sizeof(keyData[0])) {
                bool altgr = mod_ralt || (mod_lalt && mod_rctrl); // DE AltGr detection
                bool any_shift = mod_lshift || mod_rshift;

                char base     = keyData[scancode].ascii;
                char shifted  = keyData[scancode].ascii_upper;
                char alt_char = keyData[scancode].ascii_alt;

                // Letters honor CapsLock XOR Shift
                bool is_letter = (base>='a' && base<='z') || (base>='A' && base<='Z');
                if (altgr) {
                    if (scancode < 128 && de_altgr[scancode]) letter = de_altgr[scancode];
                    else if (alt_char) letter = alt_char;
                } else if (is_letter) {
                    bool upper = capslock_on ^ any_shift;
                    if (upper) letter = (base>='a'&&base<='z') ? (char)(base - 32) : (shifted?shifted:base);
                    else       letter = (base>='A'&&base<='Z') ? (char)(base + 32) : base;
                } else {
                    letter = any_shift ? (shifted?shifted:base) : base;
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
