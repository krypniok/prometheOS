// bga_minimal.c – textmode restore + BIOS font render + B8000 copy + FPS + page flipping + rotating 3D triangle
#include <stdint.h>
#include <stdbool.h>
#include "../kernel/math.h"
#include "../drivers/ports.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/debug.h"
#include "../drivers/display.h"
#include "../drivers/video.h"
#include "../kernel/mem.h"
#include "../kernel/kernel.h"
#include "../kernel/fpu.h"

void showcursor();
extern uint32_t GetTicks(void);   // liefert ms seit start
extern int get_cursor(void);      // liefert cursor offset (0..2000)

#define BGA_IOPORT_INDEX  0x01CE
#define BGA_IOPORT_DATA   0x01CF
#define VBE_DISPI_INDEX_XRES      0x01
#define VBE_DISPI_INDEX_YRES      0x02
#define VBE_DISPI_INDEX_BPP       0x03
#define VBE_DISPI_INDEX_ENABLE    0x04
#define VBE_DISPI_INDEX_X_OFFSET  0x08
#define VBE_DISPI_INDEX_Y_OFFSET  0x09
#define VBE_DISPI_DISABLED        0x00
#define VBE_DISPI_ENABLED         0x01
#define VBE_DISPI_LFB_ENABLED     0x40
#define VBE_DISPI_NOCLEARMEM      0x80

#define TEXT_COLS 80
#define TEXT_ROWS 25

#define BLINK_WAIT 250

static uint8_t saved_font[4096];          // 256 * 16 bytes
static uint16_t txtbuf[TEXT_COLS*TEXT_ROWS]; // copy of B8000 text buffer
static int saved_cursor_offset = 0;

/* -------- BGA/VGA code -------- */

static inline void ac_flipflop_reset(void) { (void)port_byte_in(0x3DA); }

static void bga_write_reg(uint16_t index, uint16_t value) {
    port_word_out(BGA_IOPORT_INDEX, index);
    port_word_out(BGA_IOPORT_DATA, value);
}

static void bga_set_mode(uint16_t w, uint16_t h, uint16_t bpp) {
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write_reg(VBE_DISPI_INDEX_XRES, w);
    bga_write_reg(VBE_DISPI_INDEX_YRES, h);
    bga_write_reg(VBE_DISPI_INDEX_BPP,  bpp);
    uint16_t flags = VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED | VBE_DISPI_NOCLEARMEM;
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, flags);
}

static void bga_disable(void) {
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
}

static uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = ((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    port_long_out(0xCF8, address);
    return port_long_in(0xCFC);
}

static uint32_t bga_get_framebuffer(void) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id = pci_config_read_dword(0, slot, 0, 0x00);
        uint16_t vendor = id & 0xffff;
        uint16_t device = id >> 16;
        if (vendor == 0x1234 && device == 0x1111) {
            uint32_t bar0 = pci_config_read_dword(0, slot, 0, 0x10);
            return bar0 & 0xfffffff0;
        }
    }
    return 0;
}

static void bga_set_display_start(uint16_t x, uint16_t y) {
    bga_write_reg(VBE_DISPI_INDEX_X_OFFSET, x);
    bga_write_reg(VBE_DISPI_INDEX_Y_OFFSET, y);
}

/* -------- VGA restore code -------- */

static const unsigned char vga_regs_80x25[] = {
    0x67,
    0x00,0x03,0x00,0x07,
    0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x50,
    0x9C,0x0E,0x8F,0x28,0x40,0x96,0xB9,0xA3,0xFF,
    0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x00,0xFF,
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x41,0x00,0x0F,0x00,0x00
};

static void vga_set_text_mode_80x25(void) {
    const unsigned char *p = vga_regs_80x25;
    port_byte_out(0x3C2, *p++);
    port_byte_out(0x3C4, 0x00); port_byte_out(0x3C5, 0x00);
    for (int i = 1; i <= 4; i++) { port_byte_out(0x3C4, i); port_byte_out(0x3C5, *p++); }
    port_byte_out(0x3C4, 0x00); port_byte_out(0x3C5, 0x03);
    port_byte_out(0x3D4, 0x03); port_byte_out(0x3D5, port_byte_in(0x3D5) | 0x80);
    port_byte_out(0x3D4, 0x11); port_byte_out(0x3D5, port_byte_in(0x3D5) & ~0x80);
    for (int i = 0; i < 25; i++) { port_byte_out(0x3D4, i); port_byte_out(0x3D5, *p++); }
    for (int i = 0; i < 9; i++) { port_byte_out(0x3CE, i); port_byte_out(0x3CF, *p++); }
    for (int i = 0; i < 21; i++) {
        ac_flipflop_reset(); port_byte_out(0x3C0, i); port_byte_out(0x3C0, *p++);
    }
    ac_flipflop_reset(); port_byte_out(0x3C0, 0x20);
}

static void vga_init_dac_default16(void) {
    static const uint8_t pal16[16][3] = {
        {0,0,0},{0,0,42},{0,42,0},{0,42,42},{42,0,0},{42,0,42},{21,21,21},{42,42,42},
        {21,21,21},{21,21,63},{21,63,21},{21,63,63},{63,21,21},{63,21,63},{63,63,21},{63,63,63}
    };
    port_byte_out(0x3C8, 0);
    for (int i=0;i<16;i++) {
        port_byte_out(0x3C9, pal16[i][0]);
        port_byte_out(0x3C9, pal16[i][1]);
        port_byte_out(0x3C9, pal16[i][2]);
    }
}

/* -------- BIOS font render to framebuffer -------- */

static void draw_glyph(uint32_t *lfb,int fbw,int x,int y,uint8_t ch,uint32_t fg,uint32_t bg,int transparent){
    const uint8_t *glyph = &saved_font[(int)ch*16]; // 8x16 font
    for(int row=0; row<16; row++){
        uint8_t bits = glyph[row];
        for(int col=0; col<8; col++){
            int on = bits & (0x80>>col);
            if(on) lfb[(y+row)*fbw + (x+col)] = fg;
            else if(!transparent) lfb[(y+row)*fbw + (x+col)] = bg;
        }
    }
}
static void draw_text(uint32_t *lfb,int fbw,int x,int y,const char *s,uint32_t fg,uint32_t bg,int transparent){
    int cx=x, cy=y;
    while(*s){
        if(*s=='\n'){ cy+=16; cx=x; s++; continue; }
        draw_glyph(lfb,fbw,cx,cy,(uint8_t)*s,fg,bg,transparent);
        cx+=9; // spacing
        s++;
    }
}

/* -------- B8000 copy + render -------- */

static void copy_b8000_to_buf(void){
    volatile uint16_t *vram = (uint16_t*)0xB8000;
    for(int i=0;i<TEXT_COLS*TEXT_ROWS;i++) txtbuf[i]=vram[i];
}

static void render_textbuf(uint32_t *lfb,int fbw,uint32_t fg,uint32_t bg){
    for(int row=0; row<TEXT_ROWS; row++){
        for(int col=0; col<TEXT_COLS; col++){
            uint16_t cell = txtbuf[row*TEXT_COLS+col];
            uint8_t ch = cell & 0xFF;
            draw_glyph(lfb,fbw,col*9,row*16,ch,fg,bg,1);
        }
    }
}

/* -------- FPS -------- */

static void draw_fps(uint32_t *lfb,int fbw,double fps){
    char buf[32];
    int n=0;
    buf[n++]='F'; buf[n++]='P'; buf[n++]='S'; buf[n++]=':'; buf[n++]=' ';
    int ip=(int)fps;
    char tmp[16]; int ti=0;
    if(ip==0){ tmp[ti++]='0'; }
    else { while(ip>0){ tmp[ti++]='0'+(ip%10); ip/=10; } }
    while(ti--) buf[n++]=tmp[ti];
    buf[n]=0;
    draw_text(lfb,fbw, fbw-9*n, 0, buf, 0x00FFFFFF,0x00000000,1);
}

/* -------- Textmode restore -------- */

int txtmode(void) {
    debug_puts("txtmode: start\n");
    bga_disable();
    vga_set_text_mode_80x25();
    vga_init_dac_default16();
    load_vga_font(saved_font, 16);

    volatile uint16_t *vram = (uint16_t*)0xB8000;
    for (int i=0;i<TEXT_COLS*TEXT_ROWS;i++) {
        vram[i] = txtbuf[i] ? txtbuf[i] : 0x0720;
    }

    set_cursor(saved_cursor_offset);
    showcursor();
    debug_puts("txtmode: end\n");
    return 0;
}


typedef struct { float x,y,z; uint32_t color; } Vertex;

static inline uint8_t getR(uint32_t c){ return (c>>16)&0xFF; }
static inline uint8_t getG(uint32_t c){ return (c>>8)&0xFF; }
static inline uint8_t getB(uint32_t c){ return c&0xFF; }

static inline uint32_t makeRGB(uint8_t r,uint8_t g,uint8_t b){
    return (r<<16)|(g<<8)|b;
}

/* -------- primitive drawing -------- */

static inline void putpix(uint32_t *lfb,int fbw,int x,int y,uint32_t color){
    lfb[y*fbw+x]=color;
}

static void draw_bresenham_line(uint32_t *lfb,int fbw,int x0,int y0,int x1,int y1,uint32_t color){
    int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2;

    while(1){
        if(x0>=0 && y0>=0) putpix(lfb,fbw,x0,y0,color);
        if(x0==x1 && y0==y1) break;
        e2 = 2*err;
        if(e2 >= dy){ err += dy; x0 += sx; }
        if(e2 <= dx){ err += dx; y0 += sy; }
    }
}

void draw_triangle_gradient(uint32_t *lfb,int fbw,
                            Vertex v0, Vertex v1, Vertex v2) {
    // 2D projektion von 3D-verts
    int x0 = (int)v0.x;
    int y0 = (int)v0.y;
    int x1 = (int)v1.x;
    int y1 = (int)v1.y;
    int x2 = (int)v2.x;
    int y2 = (int)v2.y;

    // bounding box
    int minx = (x0<x1? x0:x1); minx = (minx<x2? minx:x2);
    int miny = (y0<y1? y0:y1); miny = (miny<y2? miny:y2);
    int maxx = (x0>x1? x0:x1); maxx = (maxx>x2? maxx:x2);
    int maxy = (y0>y1? y0:y1); maxy = (maxy>y2? maxy:y2);

    float denom = (float)((y1 - y2)*(x0 - x2) + (x2 - x1)*(y0 - y2));

    for(int y=miny; y<=maxy; y++){
        for(int x=minx; x<=maxx; x++){
            float a = ((y1 - y2)*(x - x2) + (x2 - x1)*(y - y2)) / denom;
            float b = ((y2 - y0)*(x - x2) + (x0 - x2)*(y - y2)) / denom;
            float c = 1.0f - a - b;
            if(a>=0 && b>=0 && c>=0){
                int r = a*getR(v0.color) + b*getR(v1.color) + c*getR(v2.color);
                int g = a*getG(v0.color) + b*getG(v1.color) + c*getG(v2.color);
                int bcol = a*getB(v0.color) + b*getB(v1.color) + c*getB(v2.color);
                lfb[y*fbw + x] = makeRGB(r,g,bcol);
            }
        }
    }
}


/* -------- 3D rotation -------- */
static void rotate3D2(Vertex *v,float ax,float ay,float az){
    float x=v->x, y=v->y, z=v->z;
    float sx=fpu_sin(ax), cx=fpu_cos(ax);
    float sy=fpu_sin(ay), cy=fpu_cos(ay);
    float sz=fpu_sin(az), cz=fpu_cos(az);

    // rotation um X
    float y1 = y*cx - z*sx;
    float z1 = y*sx + z*cx;
    y=y1; z=z1;

    // rotation um Y
    float x2 = x*cy + z*sy;
    float z2 = -x*sy + z*cy;
    x=x2; z=z2;

    // rotation um Z
    float x3 = x*cz - y*sz;
    float y3 = x*sz + y*cz;
    x=x3; y=y3;

    v->x=x; v->y=y; v->z=z;
}

void rotate3D(Vertex *v, float ax, float ay, float az) {
    double x = v->x;
    double y = v->y;
    double z = v->z;

    // rotation um X
    double cy = fpu_cos(ax), sy = fpu_sin(ax);
    double y1 = y*cy - z*sy;
    double z1 = y*sy + z*cy;

    // rotation um Y
    double cx = fpu_cos(ay), sx = fpu_sin(ay);
    double x2 = x*cx + z1*sx;
    double z2 = -x*sx + z1*cx;

    // rotation um Z
    double cz = fpu_cos(az), sz = fpu_sin(az);
    double x3 = x2*cz - y1*sz;
    double y3 = x2*sz + y1*cz;

    v->x = (float)x3;
    v->y = (float)y3;
    v->z = (float)z2;

    // debug
    char buf[128];
    sprintf(buf,"rot3D out: x=%f y=%f z=%f\n", v->x,v->y,v->z);
    debug_puts(buf);
}


/* -------- Demo with page flipping -------- */

int bgademo2(void) {
    debug_puts("bga_minimal_demo: start\n");
	
    save_vga_font(saved_font, 16);

    debug_puts("init sin table\n");
	//init_sin_table();

    saved_cursor_offset = get_cursor();
    copy_b8000_to_buf();

    uint16_t width=720, height=400, bpp=32;
    bga_set_mode(width,height,bpp);

    uint32_t fb = bga_get_framebuffer();
    if (fb) {
        volatile uint32_t *lfb = (uint32_t*)map_framebuffer(fb, fb, 8*1024*1024);

        int current_page = 0;
        uint32_t frame_pixels = width*height;

        uint32_t start=GetTicks();
        int frames=0;
        int cursor_visible=1;
        uint32_t last_blink=GetTicks();

        float ax=0, ay=0, az=0;

        while(1) {
            int next_page = 1 - current_page;
            volatile uint32_t *drawbuf = lfb + next_page*frame_pixels;

            // gradient background
            for (uint16_t y=0;y<height;y++)
                for(uint16_t x=0;x<width;x++){
                    uint8_t r=(uint32_t)x*255/width;
                    uint8_t g=(uint32_t)y*255/height;
                    uint8_t b=(uint32_t)(x+y)*255/(width+height);
                    drawbuf[x+y*width]=(r<<16)|(g<<8)|b;
                }

            // render text buffer
            render_textbuf((uint32_t*)drawbuf,width,0x00FFFFFF,0x00000000);
            int mx, my;
            mouse_get_position(&mx, &my);
            if(mx>width-8) mx=width-8;
            if(my>height-16) my=height-16;
            draw_text((uint32_t*)drawbuf, width, mx, my, "X", 0x00FFFFFF, 0x00000000, 1);

            // 3d triangle vertices
            Vertex v0={-50,-50, 100, 0xFF0000};
            Vertex v1={ 50,-50, 100, 0x00FF00};
            Vertex v2={  0, 50, 100, 0x0000FF};

            rotate3D(&v0,ax,ay,az);
            rotate3D(&v1,ax,ay,az);
            rotate3D(&v2,ax,ay,az);

            // simple perspective
            float dist=200.0f, fov=200.0f;
            v0.x=width/2 + (fov*v0.x)/(v0.z+dist);
            v0.y=height/2+ (fov*v0.y)/(v0.z+dist);
            v1.x=width/2 + (fov*v1.x)/(v1.z+dist);
            v1.y=height/2+ (fov*v1.y)/(v1.z+dist);
            v2.x=width/2 + (fov*v2.x)/(v2.z+dist);
            v2.y=height/2+ (fov*v2.y)/(v2.z+dist);

            draw_triangle_gradient((uint32_t*)drawbuf,width,v0,v1,v2);

            // blink cursor
            uint32_t now=GetTicks();
            if(now-last_blink> BLINK_WAIT){
                cursor_visible=!cursor_visible;
                last_blink=now;
            }
            if(cursor_visible){
                int cur = get_cursor() / 2;
                int cx = (cur % TEXT_COLS) * 8;
                int cy = (cur / TEXT_COLS) * 16;
                for(int yy=0; yy<16; yy++){
                    for(int xx=0; xx<8; xx++){
                        uint32_t *pix=&drawbuf[(cy+yy)*width+(cx+xx)];
                        *pix=~(*pix);
                    }
                }
            }

            // fps
            frames++;
            uint32_t dt=now-start;
            if(dt>0){
                double fps=(frames*1000.0)/dt;
                draw_fps((uint32_t*)drawbuf,width,fps);
            }

            // rotate weiter
            ax+=0.02f; ay+=0.03f; az+=0.015f;

            // flip to new page
            bga_set_display_start(0, next_page*height);
            current_page = next_page;

            unsigned int key = getkey();
            unsigned int chr = char_from_key(key);
            if(key==SC_ESC||key==(SC_ESC|0x80)) break;

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
                printf("%c ", 0x10);
            }
            else {
                append(kernel_console_key_buffer, chr);
                char str[2] = {chr, '\0'};
                print_string(str);
            }
        }
    }

    txtmode();
    return 0;
}

/* -------- Demo with page flipping -------- */

int bgademo(void) {

    debug_puts("bga_minimal_demo: start\n");
    save_vga_font(saved_font, 16);

    saved_cursor_offset = get_cursor();
    copy_b8000_to_buf();

    uint16_t width=720, height=400, bpp=32;
    bga_set_mode(width,height,bpp);

    uint32_t fb = bga_get_framebuffer();
    if (fb) {
        volatile uint32_t *lfb = (uint32_t*)map_framebuffer(fb, fb, 8*1024*1024);

        int current_page = 0;
        uint32_t frame_pixels = width*height;

        uint32_t start=GetTicks();
        int frames=0;
        int cursor_visible=1;
        uint32_t last_blink=GetTicks();

        float ax=0, ay=0, az=0;

        // *** basis-dreieck unverändert ***
        Vertex base0={-50,-50,0,0xFF0000};
        Vertex base1={ 50,-50,0,0x00FF00};
        Vertex base2={  0, 50,0,0x0000FF};

        while(1) {
            int next_page = 1 - current_page;
            volatile uint32_t *drawbuf = lfb + next_page*frame_pixels;

            // gradient background
            for (uint16_t y=0;y<height;y++)
                for(uint16_t x=0;x<width;x++){
                    uint8_t r=(uint32_t)x*255/width;
                    uint8_t g=(uint32_t)y*255/height;
                    uint8_t b=(uint32_t)(x+y)*255/(width+height);
                    drawbuf[x+y*width]=(r<<16)|(g<<8)|b;
                }

            // render text buffer
            render_textbuf((uint32_t*)drawbuf,width,0x00FFFFFF,0x00000000);
            int mx, my;
            mouse_get_position(&mx, &my);
            if(mx>width-8) mx=width-8;
            if(my>height-16) my=height-16;
            draw_text((uint32_t*)drawbuf, width, mx, my, "X", 0x00FFFFFF, 0x00000000, 1);

            // *** kopien erzeugen und rotieren ***
            Vertex v0=base0, v1=base1, v2=base2;

           rotate3D(&v0,ax,ay,az);
           rotate3D(&v1,ax,ay,az);
           rotate3D(&v2,ax,ay,az);
		
		   char str[80];
		   sprintf(str, "v0: x=%f y=%f z=%f\n", v0.x, v0.y, v0.z);
		   debug_puts(str);


            // *** in die tiefe verschieben ***
            v0.z += 200;
            v1.z += 200;
            v2.z += 200;

            // *** perspektivische projektion ***
            float fov=200.0f;
            v0.x=width/2 + (fov*v0.x)/(v0.z);
            v0.y=height/2+ (fov*v0.y)/(v0.z);
            v1.x=width/2 + (fov*v1.x)/(v1.z);
            v1.y=height/2+ (fov*v1.y)/(v1.z);
            v2.x=width/2 + (fov*v2.x)/(v2.z);
            v2.y=height/2+ (fov*v2.y)/(v2.z);

            // zeichnen
            draw_triangle_gradient((uint32_t*)drawbuf,width,v0,v1,v2);

            // blink cursor
            uint32_t now=GetTicks();
            if(now-last_blink> BLINK_WAIT){
                cursor_visible=!cursor_visible;
                last_blink=now;
            }
            if(cursor_visible){
                int cur = get_cursor() / 2;
                int cx = (cur % TEXT_COLS) * 8;
                int cy = (cur / TEXT_COLS) * 16;
                for(int yy=0; yy<16; yy++){
                    for(int xx=0; xx<8; xx++){
                        uint32_t *pix=&drawbuf[(cy+yy)*width+(cx+xx)];
                        *pix=~(*pix);
                    }
                }
            }

            // fps
            frames++;
            uint32_t dt=now-start;
            if(dt>0){
                double fps=(frames*1000.0)/dt;
                draw_fps((uint32_t*)drawbuf,width,fps);
            }

            // *** winkel hochzählen ***
            ax+=0.02f;
            ay+=0.03f;
            az+=0.015f;

            // flip to new page
            bga_set_display_start(0, next_page*height);
            current_page = next_page;

            unsigned int key = getkey_async();
            unsigned int chr = char_from_key(key);
            if(key==SC_ESC||key==(SC_ESC|0x80)) break;

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
                printf("%c ", 0x10);
            }
            else {
                append(kernel_console_key_buffer, chr);
                char str[2] = {chr, '\0'};
                print_string(str);
            }
        }
    }

    txtmode();
    return 0;
}
