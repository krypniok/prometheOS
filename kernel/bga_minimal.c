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
#include "../cpu/timer.h"
#include "../kernel/mem.h"
#include "../kernel/kernel.h"
#include "../kernel/fpu.h"
#include "../stdlibs/homebrewdb.h"
#include "../stdlibs/stdio_fs.h"
#include "../stdlibs/memory.h"
#include "../stdlibs/bmp.h"

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

static uint8_t saved_font[8192];          // 256 * up to 32 bytes
static int font_rows = 16;
static int g_bga_active = 0;              // prevent re-entry of demo while running
static uint16_t txtbuf[TEXT_COLS*TEXT_ROWS]; // copy of shadow text buffer
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
    const uint8_t *glyph = &saved_font[(int)ch*font_rows]; // 8xN font
    for(int row=0; row<font_rows; row++){
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
        if(*s=='\n'){ cy+=font_rows; cx=x; s++; continue; }
        draw_glyph(lfb,fbw,cx,cy,(uint8_t)*s,fg,bg,transparent);
        cx+=9; // spacing
        s++;
    }
}

/* -------- B8000 copy + render -------- */

static void copy_shadow_to_buf(void){
    uint16_t *sh = console_get_shadow();
    for(int i=0;i<TEXT_COLS*TEXT_ROWS;i++) txtbuf[i]=sh[i];
}

static void render_textbuf(uint32_t *lfb,int fbw,uint32_t fg,uint32_t bg){
    for(int row=0; row<TEXT_ROWS; row++){
        for(int col=0; col<TEXT_COLS; col++){
            uint16_t cell = txtbuf[row*TEXT_COLS+col];
            uint8_t ch = cell & 0xFF;
            draw_glyph(lfb,fbw,col*9,row*font_rows,ch,fg,bg,1);
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
    // Restore currently active font (rows) back into text mode
    load_vga_font(saved_font, (size_t)font_rows);

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


void blit_bmp_scaled(uint32_t *dst, int dst_w, int dst_h,
                     uint8_t *bmp_pixels, int src_w, int src_h,
                     int dst_x, int dst_y, int dst_wout, int dst_hout) {
    (void)dst_h; // unused
    for (int y = 0; y < dst_hout; y++) {
        int sy = (y * src_h) / dst_hout;
        for (int x = 0; x < dst_wout; x++) {
            int sx = (x * src_w) / dst_wout;
            const uint8_t* sp = bmp_pixels + ((sy*src_w + sx) * 4); // RGBA
            uint8_t r = sp[0], g = sp[1], b = sp[2];
            dst[(dst_y + y) * dst_w + (dst_x + x)] = (r<<16) | (g<<8) | b;
        }
    }
}

// Blit a sub-rectangle from a 32-bit BMP into dst with scaling
void blit_bmp_region_scaled(uint32_t *dst, int fb_w, int fb_h,
                            uint8_t *bmp_pixels, int img_w, int img_h,
                            int src_x, int src_y, int src_w, int src_h,
                            int dst_x, int dst_y, int dst_w, int dst_h) {
    if (!dst || !bmp_pixels) return;
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    if (src_x < 0) { src_w += src_x; src_x = 0; }
    if (src_y < 0) { src_h += src_y; src_y = 0; }
    if (src_x+src_w > img_w) src_w = img_w - src_x;
    if (src_y+src_h > img_h) src_h = img_h - src_y;
    if (src_w <= 0 || src_h <= 0) return;

    for (int y = 0; y < dst_h; y++) {
        int sy = src_y + (y * src_h) / dst_h;
        int dy = dst_y + y; if (dy < 0 || dy >= fb_h) continue;
        for (int x = 0; x < dst_w; x++) {
            int sx = src_x + (x * src_w) / dst_w;
            int dx = dst_x + x; if (dx < 0 || dx >= fb_w) continue;
            const uint8_t* sp = bmp_pixels + ((sy*img_w + sx) * 4); // RGBA
            uint8_t r = sp[0], g = sp[1], b = sp[2];
            dst[dy*fb_w + dx] = (r<<16) | (g<<8) | b;
        }
    }
}

// Same as above, but skip writes for a given color key (0x00RRGGBB)
void blit_bmp_region_scaled_ckey(uint32_t *dst, int fb_w, int fb_h,
                                 uint8_t *bmp_pixels, int img_w, int img_h,
                                 int src_x, int src_y, int src_w, int src_h,
                                 int dst_x, int dst_y, int dst_w, int dst_h,
                                 uint32_t color_key) {
    if (!dst || !bmp_pixels) return;
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    if (src_x < 0) { src_w += src_x; src_x = 0; }
    if (src_y < 0) { src_h += src_y; src_y = 0; }
    if (src_x+src_w > img_w) src_w = img_w - src_x;
    if (src_y+src_h > img_h) src_h = img_h - src_y;
    if (src_w <= 0 || src_h <= 0) return;

    uint32_t *src = (uint32_t*)bmp_pixels;
    for (int y = 0; y < dst_h; y++) {
        int sy = src_y + (y * src_h) / dst_h;
        int dy = dst_y + y; if (dy < 0 || dy >= fb_h) continue;
        for (int x = 0; x < dst_w; x++) {
            int sx = src_x + (x * src_w) / dst_w;
            int dx = dst_x + x; if (dx < 0 || dx >= fb_w) continue;
            uint32_t c = src[sy*img_w + sx];
            if (c != color_key) dst[dy*fb_w + dx] = c;
        }
    }
}

// Alpha-blended blit from RGBA source onto 0x00RRGGBB framebuffer
void blit_bmp_region_scaled_alpha(uint32_t *dst, int fb_w, int fb_h,
                                  uint8_t *bmp_pixels, int img_w, int img_h,
                                  int src_x, int src_y, int src_w, int src_h,
                                  int dst_x, int dst_y, int dst_w, int dst_h) {
    if (!dst || !bmp_pixels) return;
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    if (src_x < 0) { src_w += src_x; src_x = 0; }
    if (src_y < 0) { src_h += src_y; src_y = 0; }
    if (src_x+src_w > img_w) src_w = img_w - src_x;
    if (src_y+src_h > img_h) src_h = img_h - src_y;
    if (src_w <= 0 || src_h <= 0) return;

    for (int y = 0; y < dst_h; y++) {
        int sy = src_y + (y * src_h) / dst_h;
        int dy = dst_y + y; if (dy < 0 || dy >= fb_h) continue;
        for (int x = 0; x < dst_w; x++) {
            int sx = src_x + (x * src_w) / dst_w;
            int dx = dst_x + x; if (dx < 0 || dx >= fb_w) continue;
            const uint8_t* sp = bmp_pixels + ((sy*img_w + sx) * 4);
            uint8_t sr = sp[0], sg = sp[1], sb = sp[2], sa = sp[3];
            if (sa == 0) continue; // fully transparent
            uint32_t* dp = &dst[dy*fb_w + dx];
            if (sa == 255) { *dp = (sr<<16)|(sg<<8)|sb; continue; }
            uint32_t d = *dp;
            uint8_t dr = (d>>16)&0xFF, dg = (d>>8)&0xFF, db = d&0xFF;
            // standard alpha blend: out = s*a + d*(1-a)
            uint32_t a = sa; uint32_t ia = 255 - a;
            uint8_t rr = (uint8_t)((sr*a + dr*ia) / 255);
            uint8_t rg = (uint8_t)((sg*a + dg*ia) / 255);
            uint8_t rb = (uint8_t)((sb*a + db*ia) / 255);
            *dp = (rr<<16)|(rg<<8)|rb;
        }
    }
}

/*
void gradient_buf() {
    // gradient background
    for (uint16_t y=0;y<height;y++)
        for(uint16_t x=0;x<width;x++) {
 	    	uint8_t r=(uint32_t)x*255/width;
            uint8_t g=(uint32_t)y*255/height;
            uint8_t b=(uint32_t)(x+y)*255/(width+height);
            drawbuf[x+y*width]=(r<<16)|(g<<8)|b;
         }
}
*/

void clear_lfb_buffer(uint32_t *lfb, int width, int height) {
    //memset(lfb, 0, width * height * sizeof(uint32_t));
    for (uint16_t y=0;y<height;y++)
        for(uint16_t x=0;x<width;x++) {
 	    	//uint8_t r=(uint32_t)x*255/width;
            //uint8_t g=(uint32_t)y*255/height;
            //uint8_t b=(uint32_t)(x+y)*255/(width+height);
            lfb[x+y*width]=0; //(r<<16)|(g<<8)|b;
         }
}

/* -------- Demo with page flipping -------- */
int bgademo(void) {

	if (g_bga_active) {
		// Already active – avoid nested init/loop
		printf("BGA already active. Press ESC to exit demo.\n");
		return 0;
	}
	g_bga_active = 1;

	uint32_t lastFrame = GetTicks();

    debug_puts("bga_minimal_demo: start\n");
    font_rows = (int)vga_get_font_height(); if (font_rows<=0 || font_rows>32) font_rows=16;
    save_vga_font(saved_font, (size_t)font_rows);

    saved_cursor_offset = get_cursor();
    // Ensure a fresh prompt is visible during demo; notify kernel loop to skip its default prompt once
    extern int g_skip_prompt_once;
    printf("%c ", 0x10);
    g_skip_prompt_once = 1;
    copy_shadow_to_buf();

    // Global background image holder (freed at end)
    Bmp32 bg = (Bmp32){0};
    Bmp32 spr = (Bmp32){0};

    uint16_t width=720, height=400, bpp=32;
    bga_set_mode(width,height,bpp);

    uint32_t fb = bga_get_framebuffer();
    if (fb) {
        volatile uint32_t *lfb = (uint32_t*)map_framebuffer(fb, fb, 8*1024*1024);

        int current_page = 0;
        uint32_t frame_pixels = width*height;

        // Clear both pages to avoid showing garbage after enabling LFB
        for (int p = 0; p < 2; p++) {
            volatile uint32_t *page = lfb + p*frame_pixels;
            for (uint32_t i = 0; i < frame_pixels; i++) page[i] = 0;
        }
        // Draw an immediate initial frame (text snapshot over black) on page 0
        copy_shadow_to_buf();
        render_textbuf((uint32_t*)lfb, width, 0x00FFFFFF, 0x00000000);
        // Draw software cursor initially (underscore at current cursor cell)
        {
            int cur = get_cursor() / 2;
            int cx = (cur % TEXT_COLS) * 9;
            int cy = (cur / TEXT_COLS) * 16;
            draw_glyph((uint32_t*)lfb, width, cx, cy, '_', 0x00FFFFFF, 0x00000000, 1);
        }
        bga_set_display_start(0, 0);

    // FPS timing and blink state
    int cursor_visible=1;
    uint32_t last_blink=GetTicks();
    const uint32_t FRAME_MS = 33; // ~30 FPS cap to avoid flicker in QEMU
    // Instantaneous + smoothed FPS (avoid long-term average that starts low)
    double fps_smooth = 0.0;

    // Try to load background BMP from HomebrewDB (logo.bmp) via stdio
    FILE* f = fopen("logo.bmp", "rb");
    if (f) { if (!load_bmp(f, &bg)) { /* ignore */ } fclose(f); }
    // Try to load sprite sheet with alpha (explosion2.bmp), fallback to sprite.bmp then logo
    FILE* fs = fopen("explosion2.bmp", "rb");
    if (fs) { if (!load_bmp(fs, &spr)) { /* ignore */ } fclose(fs); }
    if (!spr.pixels) {
        fs = fopen("sprite.bmp", "rb");
        if (fs) { if (!load_bmp(fs, &spr)) { /* ignore */ } fclose(fs); }
    }
    if (!spr.pixels && bg.pixels) { spr = bg; } // last resort fallback
    uint8_t* bmp_pixels = bg.pixels; int src_w = bg.width; int src_h = bg.height;

    // Pre-render a full-frame background once to avoid per-frame scaling cost
    uint32_t* bg_frame = 0;
    if (bmp_pixels && src_w>0 && src_h>0) {
        bg_frame = (uint32_t*)malloc(frame_pixels * 4);
        if (bg_frame) {
            // Fill black
            memset(bg_frame, 0, frame_pixels * 4);
            // Compute letterbox placement (keep aspect)
            int sw = src_w, sh = src_h; int dw = width, dh = height, dx = 0, dy = 0;
            if (width * sh < height * sw) { dw = width; dh = (width * sh) / sw; dx = 0; dy = (height - dh) / 2; }
            else { dh = height; dw = (height * sw) / sh; dy = 0; dx = (width - dw) / 2; }
            blit_bmp_scaled(bg_frame, width, height, bmp_pixels, sw, sh, dx, dy, dw, dh);
        }
    }

    // Sprite animation state (C64 sprite me(tm) style)
    int frames_x = spr.pixels ? 5 : 4;
    int frames_y = spr.pixels ? 5 : 4;
    int frames_total = frames_x * frames_y;
    int tile_w = spr.pixels ? (spr.width / frames_x) : 0;
    int tile_h = spr.pixels ? (spr.height / frames_y) : 0;
    int anim_frame = 0; uint32_t last_anim = GetTicks(); const uint32_t ANIM_MS = 100;
    // Scale sprite to a nice on-screen size
    int sp_w = (tile_w > 0) ? (width / 6) : 0;
    int sp_h = (tile_h > 0) ? (sp_w * tile_h) / (tile_w ? tile_w : 1) : 0;
    if (sp_h <= 0) sp_h = sp_w; if (sp_w <= 0) sp_w = 0;
    int sp_x = width/3, sp_y = height/3; int vx = 4, vy = 3;

    while(1) {
            // Measure time since previous frame (includes prior sleep)
            uint32_t now0 = GetTicks();
            uint32_t dt_actual = now0 - lastFrame; if (dt_actual == 0) dt_actual = 1;
            lastFrame = now0;

            uint32_t frame_begin = now0;
            int next_page = 1 - current_page;
            volatile uint32_t *drawbuf = lfb + next_page*frame_pixels;


			//clear_lfb_buffer((uint32_t*)drawbuf, width, height);


// Hintergrund kopieren (prerendered) oder, falls nicht vorhanden, leer lassen
if (bg_frame) {
    memcpy((void*)drawbuf, bg_frame, frame_pixels * 4);
}


            // Refresh text snapshot from shadow and render
            copy_shadow_to_buf();
            render_textbuf((uint32_t*)drawbuf,width,0x00FFFFFF,0x00000000);

            // Sprite animation + bounce
            if (spr.pixels && tile_w>0 && tile_h>0 && sp_w>0 && sp_h>0) {
                // advance animation at fixed rate
                uint32_t n = GetTicks();
                if (n - last_anim >= ANIM_MS) { anim_frame = (anim_frame + 1) % frames_total; last_anim = n; }
                // update position
                sp_x += vx; sp_y += vy;
                if (sp_x < 0) { sp_x = 0; vx = -vx; }
                if (sp_y < 0) { sp_y = 0; vy = -vy; }
                if (sp_x + sp_w >= width)  { sp_x = width - sp_w;  vx = -vx; }
                if (sp_y + sp_h >= height) { sp_y = height - sp_h; vy = -vy; }
                // compute source rect
                int fx = (anim_frame % frames_x) * tile_w;
                int fy = (anim_frame / frames_x) * tile_h;
                // Alpha blended sprite over background (RGBA source)
                blit_bmp_region_scaled_alpha((uint32_t*)drawbuf, width, height,
                                             spr.pixels, spr.width, spr.height,
                                             fx, fy, tile_w, tile_h,
                                             sp_x, sp_y, sp_w, sp_h);
            }
            int mx, my;
            mouse_get_position(&mx, &my);
            if(mx>width-8) mx=width-8;
            if(my>height-16) my=height-16;
            draw_text((uint32_t*)drawbuf, width, mx, my, "X", 0x00FFFFFF, 0x00000000, 1);

            // blink cursor
            uint32_t now=GetTicks();
            if(now-last_blink> BLINK_WAIT){
                cursor_visible=!cursor_visible;
                last_blink=now;
            }
            if(cursor_visible){
                int cur = get_cursor() / 2;
                int cx = (cur % TEXT_COLS) * 9;
                int cy = (cur / TEXT_COLS) * 16;
                // Draw an underscore glyph at the cursor cell (transparent bg)
                draw_glyph((uint32_t*)drawbuf, width, cx, cy, '_', 0x00FFFFFF, 0x00000000, 1);
            }
            // FPS: instantaneous based on previous frame duration, with light smoothing
            double fps_inst = 1000.0 / (double)dt_actual;
            if (fps_smooth == 0.0) fps_smooth = fps_inst;
            // Exponential moving average
            fps_smooth = 0.2 * fps_inst + 0.8 * fps_smooth;
            draw_fps((uint32_t*)drawbuf,width,fps_smooth);

			// warte bis retrace beginnt (BGA sucks)
			//while (port_byte_in(0x3DA) & 0x08);
			//while (!(port_byte_in(0x3DA) & 0x08));



            // flip to new page
            bga_set_display_start(0, next_page*height);
            current_page = next_page;

            unsigned int key = getkey_async();
            if (key != 0) {
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
                else if ((key == SC_PAGEUP) || ((key>>8)==0xE0 && ((key & 0xFF) == SC_PAGEUP))) {
                    console_page_up();
                }
                else if ((key == SC_PAGEDOWN) || ((key>>8)==0xE0 && ((key & 0xFF) == SC_PAGEDOWN))) {
                    console_page_down();
                }
                else {
                    append(kernel_console_key_buffer, chr);
                    char str[2] = {chr, '\0'};
                    print_string(str);
                }
            }
            // Frame pacing to ~30 FPS
            uint32_t frame_time = GetTicks() - frame_begin;
            if (frame_time < FRAME_MS) {
                sleep(FRAME_MS - frame_time);
            }
        }
        if (bg_frame) free(bg_frame);
    }
    if (bg.pixels && spr.pixels != bg.pixels) free_bmp(&bg);
    if (spr.pixels && spr.pixels != bg.pixels) free_bmp(&spr);

    // Preserve latest cursor position and text contents when restoring text mode
    saved_cursor_offset = get_cursor();
    copy_shadow_to_buf();
    txtmode();
    g_bga_active = 0;
    return 0;
}
