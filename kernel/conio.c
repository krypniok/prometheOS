#include "../drivers/ports.h"
#include "../cpu/timer.h"

#define DTMF_0_FREQ 941
#define DTMF_1_FREQ 697
#define DTMF_2_FREQ 770
#define DTMF_3_FREQ 852
#define DTMF_4_FREQ 941
#define DTMF_5_FREQ 1209
#define DTMF_6_FREQ 1336
#define DTMF_7_FREQ 1477
#define DTMF_8_FREQ 1633
#define DTMF_9_FREQ 1776
#define DTMF_STAR_FREQ 941
#define DTMF_POUND_FREQ 1633
#define FREE_FREQ 440

// Makro zur Konvertierung der Frequenz in Hertz in die erforderlichen LSB und MSB Werte
#define FREQUENCY_TO_LSB_MSB(frequency) \
    ((uint16_t)(1193180 / (frequency)))

#define uint8_t unsigned char
#define uint16_t unsigned int

void console_sound_on() {
    // Aktiviere den PC Speaker (Bit 0 und 1 setzen)
    uint8_t prev = port_byte_in(0x61); // Vorheriger Zustand speichern
    port_byte_out(0x61, prev | 0x03); // Bit 0 und 1 setzen

}

void console_play_sound(unsigned int freq) {
    // Konvertiere die Frequenz in LSB und MSB
    uint16_t lsb_msb = FREQUENCY_TO_LSB_MSB(freq);
    // Generiere den Piepton
    port_byte_out(0x43, 0xB6); // Set Control Word
    port_byte_out(0x42, (uint8_t)(lsb_msb & 0xFF)); // LSB der Frequenz
    port_byte_out(0x42, (uint8_t)(lsb_msb >> 8));   // MSB der Frequenz
}
 
void console_sound_off() {
 	unsigned char tmp = port_byte_in(0x61) & 0xFC;
  	port_byte_out(0x61, tmp);
 }
 
void _beep(int freq, int ms) {
	console_play_sound(freq);
    sleep(ms);
}

void beep(int freq, int ms) {
	static ison = false;
	console_sound_on();
	for(int i = 0; i < ms; i++) {
		//_beep(freq, ms);
		console_play_sound(freq++);
		sleep(10);
	}
	console_sound_off();
}

// Funktion zum Konvertieren und Abspielen der Zeichenkette
void playDTMF(const char *sequence) {
    while (*sequence) {
        char digit = *sequence++;
        int frequency = 0;
        switch (digit) {
            case '0': frequency = DTMF_0_FREQ; break;
            case '1': frequency = DTMF_1_FREQ; break;
            case '2': frequency = DTMF_2_FREQ; break;
            case '3': frequency = DTMF_3_FREQ; break;
            case '4': frequency = DTMF_4_FREQ; break;
            case '5': frequency = DTMF_5_FREQ; break;
            case '6': frequency = DTMF_6_FREQ; break;
            case '7': frequency = DTMF_7_FREQ; break;
            case '8': frequency = DTMF_8_FREQ; break;
            case '9': frequency = DTMF_9_FREQ; break;
            case '*': frequency = DTMF_STAR_FREQ; break;
            case '#': frequency = DTMF_POUND_FREQ; break;
            default: frequency = FREE_FREQ; break;
        }
        beep(frequency, 50); // Spielt den Ton für 500 Millisekunden ab (kann angepasst werden).
        sleep(50); // Pausiert für 100 Millisekunden zwischen den Tönen (kann angepasst werden).
    }
}

int sscanf(const char *str, const char *format, int *arg1, int *arg2) {
    int count = 0;

    while (*str && *format) {
        if (*format == '%' && *(format + 1) == 'd') {
            if (*str >= '0' && *str <= '9') {
                int value = 0;
                while (*str >= '0' && *str <= '9') {
                    value = value * 10 + (*str - '0');
                    str++;
                }
                if (count == 0) {
                    *arg1 = value;
                } else if (count == 1) {
                    *arg2 = value;
                }
                count++;
            }
            format += 2; // Überspringen von "%d" im Format-String
        } else {
            // Übereinstimmende Zeichen im Format und in der Eingabe
            if (*str == *format) {
                str++;
                format++;
            } else {
                break;
            }
        }
    }

    return count;
}
