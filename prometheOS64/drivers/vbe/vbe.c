#include "../../include/drivers/vbe.h"

struct video_info_struct v_info;
unsigned char *framebuffer;
unsigned char *bios_font = (unsigned char *)0x6000; // BIOS Font Copy

void init_vbe(struct video_info_struct *info) {
    v_info = *info;
    framebuffer = (unsigned char *)v_info.lfb;
    
    // Fallback Pitch Berechnung
    if (v_info.pitch == 0 && v_info.width > 0 && v_info.bpp > 0) {
        v_info.pitch = v_info.width * (v_info.bpp / 8);
    }
}

// Hilfsfunktion: Pixel setzen
void put_pixel(int x, int y, unsigned int color) {
    if (x >= 0 && x < v_info.width && y >= 0 && y < v_info.height) {
        unsigned int offset = y * v_info.pitch + x * (v_info.bpp / 8);
        unsigned char *dst = framebuffer + offset;
        
        if (v_info.bpp == 32) {
            *(unsigned int*)dst = color;
        } 
        else if (v_info.bpp == 24) {
            dst[0] = color & 0xFF;
            dst[1] = (color >> 8) & 0xFF;
            dst[2] = (color >> 16) & 0xFF;
        }
    }
}

void draw_rect(int x, int y, int w, int h, unsigned int color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            put_pixel(x + j, y + i, color);
        }
    }
}

void draw_circle(int xc, int yc, int r, unsigned int color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        put_pixel(xc+x, yc+y, color);
        put_pixel(xc-x, yc+y, color);
        put_pixel(xc+x, yc-y, color);
        put_pixel(xc-x, yc-y, color);
        put_pixel(xc+y, yc+x, color);
        put_pixel(xc-y, yc+x, color);
        put_pixel(xc+y, yc-x, color);
        put_pixel(xc-y, yc-x, color);
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void draw_char(int x, int y, char c, unsigned int color) {
    unsigned char *glyph = bios_font + (unsigned char)c * 16;
    for (int i = 0; i < 16; i++) {
        unsigned char row = glyph[i];
        for (int j = 0; j < 8; j++) {
            if ((row >> (7-j)) & 1) {
                put_pixel(x + j, y + i, color);
            } else {
                put_pixel(x + j, y + i, 0x000000);
            }
        }
    }
}

void draw_number(int x, int y, int num, unsigned int color) {
    char buf[16];
    int i = 0;
    if (num == 0) { draw_char(x, y, '0', color); return; }
    while (num > 0) { buf[i++] = (num % 10) + '0'; num /= 10; }
    while (--i >= 0) { draw_char(x, y, buf[i], color); x += 9; }
}

void draw_spectrum() {
    for(int y=0; y<v_info.height; y++)
        for(int x=0; x<v_info.width; x++) put_pixel(x, y, ((x*255/v_info.width)<<16) | ((y*255/v_info.height)<<8) | ((x+y)*255/(v_info.width+v_info.height)));
}