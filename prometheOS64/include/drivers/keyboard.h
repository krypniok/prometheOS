#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_handler();
char kbd_getchar(); // Blockierend (wartet auf Taste)
int kbd_has_key();  // Prüft, ob Taste da ist (nicht blockierend)
char kbd_get_key(); // Holt Taste (nicht blockierend), gibt 0 zurück wenn leer

#endif