#ifndef MOUSE_H
#define MOUSE_H

extern volatile int mouse_x;
extern volatile int mouse_y;

void init_mouse();
void mouse_handler();

#endif