#ifndef VBE_H
#define VBE_H

// Struktur für die VBE-Informationen (vom Preloader übergeben)
struct video_info_struct {
    unsigned int lfb;
    unsigned short width;
    unsigned short height;
    unsigned short pitch;
    unsigned char bpp;
} __attribute__((packed));

extern struct video_info_struct v_info;
extern unsigned char *framebuffer;

void init_vbe(struct video_info_struct *info);
void put_pixel(int x, int y, unsigned int color);
void draw_rect(int x, int y, int w, int h, unsigned int color);
void draw_circle(int xc, int yc, int r, unsigned int color);
void draw_char(int x, int y, char c, unsigned int color);
void draw_number(int x, int y, int num, unsigned int color);
void draw_spectrum();

#endif