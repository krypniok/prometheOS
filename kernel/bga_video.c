#include <stdint.h>
#include "../drivers/ports.h"
#include "bga_video.h"
#include "../drivers/display.h" // for VGA text console helpers
#include "../stdlibs/string.h"

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

static inline void bga_write(uint16_t idx, uint16_t val){ port_word_out(BGA_IOPORT_INDEX, idx); port_word_out(BGA_IOPORT_DATA, val); }

static uint32_t g_lfb = 0;
static int g_w=0, g_h=0;
// Save current text-mode font to restore after closing BGA
static uint8_t g_saved_font[256*32];
static size_t  g_saved_font_h = 16;
static int     g_saved_cursor = 0;

static uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off){
    uint32_t addr=((bus<<16)|(slot<<11)|(func<<8)|(off & 0xFC)|0x80000000u); port_long_out(0xCF8, addr); return port_long_in(0xCFC);
}
static uint32_t find_bga_lfb(){
    for(uint8_t slot=0;slot<32;slot++){
        uint32_t id=pci_config_read_dword(0,slot,0,0x00); uint16_t ven=id&0xFFFF, dev=id>>16; if(ven==0x1234 && dev==0x1111){ uint32_t bar0=pci_config_read_dword(0,slot,0,0x10); return bar0 & 0xFFFFfff0u; }
    }
    return 0;
}

int bga_init(int w, int h){
    if (w<=0||h<=0) { w=800; h=600; }
    // Capture current text-mode font while we are still in VGA text
    g_saved_font_h = vga_get_font_height(); if (g_saved_font_h==0||g_saved_font_h>32) g_saved_font_h=16;
    g_saved_cursor = get_cursor();
    save_vga_font(g_saved_font, g_saved_font_h);
    g_lfb = find_bga_lfb(); if (!g_lfb) return -1;
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_XRES, (uint16_t)w);
    bga_write(VBE_DISPI_INDEX_YRES, (uint16_t)h);
    bga_write(VBE_DISPI_INDEX_BPP,  32);
    // Clear VRAM on mode set to avoid showing garbage; then ensure offsets = 0
    uint16_t flags = VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED; // no NOCLEARMEM
    bga_write(VBE_DISPI_INDEX_ENABLE, flags);
    // make sure display start is the first page
    bga_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    g_w=w; g_h=h; return 0;
}

// Minimal VGA text-mode restore (80x25, default 16-color DAC)
static void _vga_set_text_mode_80x25(void){
    static const unsigned char regs[] = {
        0x67,
        0x00,0x03,0x00,0x07,
        0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x50,
        0x9C,0x0E,0x8F,0x28,0x40,0x96,0xB9,0xA3,0xFF,
        0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x00,0xFF,
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
        0x41,0x00,0x0F,0x00,0x00
    };
    const unsigned char *p = regs;
    port_byte_out(0x3C2, *p++);
    port_byte_out(0x3C4, 0x00); port_byte_out(0x3C5, 0x00);
    port_byte_out(0x3C4, 0x01); port_byte_out(0x3C5, 0x01);
    port_byte_out(0x3C4, 0x02); port_byte_out(0x3C5, 0x03);
    port_byte_out(0x3C4, 0x03); port_byte_out(0x3C5, 0x00);
    port_byte_out(0x3C4, 0x04); port_byte_out(0x3C5, 0x02);
    port_byte_out(0x3D4, 0x11); port_byte_out(0x3D5, 0x0E | 0x80);
    for (int i=0;i<25;i++){ port_byte_out(0x3D4, i); port_byte_out(0x3D5, *p++); }
    for (int i=0;i<9;i++){ port_byte_out(0x3D4, i+0x10); port_byte_out(0x3D5, *p++); }
    port_byte_out(0x3D4, 0x11); port_byte_out(0x3D5, 0x0E);
    port_byte_out(0x3CE, 0x00); port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x01); port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x02); port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x03); port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x04); port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x05); port_byte_out(0x3CF, 0x10);
    port_byte_out(0x3CE, 0x06); port_byte_out(0x3CF, 0x0E);
    port_byte_out(0x3CE, 0x07); port_byte_out(0x3CF, 0x00);
    port_byte_out(0x3CE, 0x08); port_byte_out(0x3CF, 0xFF);
    // Attribute controller writes require flip-flop reset via 0x3DA
    (void)port_byte_in(0x3DA);
    port_byte_out(0x3C0, 0x30); port_byte_out(0x3C0, 0x41);
    (void)port_byte_in(0x3DA);
    port_byte_out(0x3C0, 0x33); port_byte_out(0x3C0, 0x00);
    for (int i=0;i<16;i++){ (void)port_byte_in(0x3DA); port_byte_out(0x3C0, i); port_byte_out(0x3C0, i); }
    // Unblank display
    (void)port_byte_in(0x3DA);
    port_byte_out(0x3C0, 0x20);
}
static void _vga_init_dac_default16(void){
    static const unsigned char pal16[16*3] = {
      0,0,0, 0,0,42, 0,42,0, 0,42,42, 42,0,0, 42,0,42, 42,21,0, 42,42,42,
      21,21,21, 21,21,63, 21,63,21, 21,63,63, 63,21,21, 63,21,63, 63,63,21, 63,63,63
    };
    port_byte_out(0x3C8, 0);
    for (int i=0;i<16*3;i++) port_byte_out(0x3C9, pal16[i]);
}

// Minimal diagnostic: set text mode regs and write a VRAM marker, no console helpers
void vga_diag_txtforce(void){
    bga_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    _vga_set_text_mode_80x25();
    _vga_init_dac_default16();
    volatile uint16_t* v = (uint16_t*)0xB8000;
    for (int i=0;i<80*25;i++) v[i] = (uint16_t)((0x0F<<8) | ' ');
    v[0] = (uint16_t)((0x0F << 8) | 'D');
    v[1] = (uint16_t)((0x0F << 8) | 'I');
    v[2] = (uint16_t)((0x0F << 8) | 'A');
    v[3] = (uint16_t)((0x0F << 8) | 'G');
    v[4] = (uint16_t)((0x0F << 8) | ':');
}
// debug helpers
void debug_puts(const char*);

void bga_close(void){
    debug_puts("[bga_close] enter\n");
    // Reset display start to top-left and disable BGA
    bga_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    debug_puts("[bga_close] bga disabled\n");
    _vga_set_text_mode_80x25();
    _vga_init_dac_default16();
    debug_puts("[bga_close] vga text + dac\n");
    // Restore previously saved font and console contents
    load_vga_font(g_saved_font, g_saved_font_h);
    debug_puts("[bga_close] font restored\n");
    // Ensure a visible, sane text console state
    console_page_end();            // back to live view + flush from shadow
    set_color(WHITE_ON_BLACK);     // standard attribute
    clear_screen();                // draw a clean screen (cursor at 0,0)
    console_flush_to_vga();        // force copy to VRAM
    debug_puts("[bga_close] console flushed\n");
    // Write a visible marker directly to VRAM to prove text mode is active
    {
        volatile uint16_t* v = (uint16_t*)0xB8000;
        v[0] = (uint16_t)((0x0F << 8) | 'T');
        v[1] = (uint16_t)((0x0F << 8) | 'X');
        v[2] = (uint16_t)((0x0F << 8) | 'T');
        v[3] = (uint16_t)((0x0F << 8) | ' ');
        v[4] = (uint16_t)((0x0F << 8) | 'M');
        v[5] = (uint16_t)((0x0F << 8) | 'O');
        v[6] = (uint16_t)((0x0F << 8) | 'D');
        v[7] = (uint16_t)((0x0F << 8) | 'E');
    }
    set_cursor(g_saved_cursor);    // restore cursor position
    g_lfb=0; g_w=g_h=0;
    debug_puts("[bga_close] done\n");
}

int bga_is_active(void){ return g_lfb ? 1 : 0; }
int bga_width(void){ return g_w; }
int bga_height(void){ return g_h; }

size_t bga_snapshot_size(void){
    if (!g_lfb || g_w <= 0 || g_h <= 0) return 0;
    return (size_t)g_w * (size_t)g_h * sizeof(uint32_t);
}

int bga_snapshot(void* dest, size_t bytes){
    size_t need = bga_snapshot_size();
    if (!need || !dest || bytes < need) return -1;
    memcpy(dest, (const void*)g_lfb, need);
    return 0;
}

int bga_restore_from_snapshot(int w, int h, const void* src, size_t bytes){
    if (!src || w <= 0 || h <= 0) return -1;
    size_t need = (size_t)w * (size_t)h * sizeof(uint32_t);
    if (bytes < need) return -1;
    if (bga_init(w, h) != 0) return -1;
    memcpy((void*)g_lfb, src, need);
    return 0;
}

static inline void put32(uint32_t addr, uint32_t val){ *(volatile uint32_t*)addr = val; }
static inline uint32_t lfb_addr(int x,int y){ return g_lfb + (uint32_t)(y*g_w + x)*4u; }

void bga_clear(uint32_t color){ if(!g_lfb) return; for(int y=0;y<g_h;y++){ uint32_t* p=(uint32_t*)(g_lfb + (uint32_t)y*g_w*4u); for(int x=0;x<g_w;x++) p[x]=color; } }
void bga_drawpixel(int x,int y,uint32_t c){ if(!g_lfb) return; if((unsigned)x>=(unsigned)g_w||(unsigned)y>=(unsigned)g_h) return; put32(lfb_addr(x,y), c); }

void bga_drawline(int x0,int y0,int x1,int y1,uint32_t c){
    int dx = (x1>x0)?(x1-x0):(x0-x1), sx = (x0<x1)?1:-1;
    int dy = (y1>y0)?(y0-y1):(y1-y0), sy = (y0<y1)?1:-1; // note: dy negative
    int err = dx + dy;
    while(1){ bga_drawpixel(x0,y0,c); if(x0==x1 && y0==y1) break; int e2 = 2*err; if(e2 >= dy){ err += dy; x0 += sx; } if(e2 <= dx){ err += dx; y0 += sy; } }
}

static void tri_edge(int x0,int y0,int x1,int y1,int* minx,int* maxx,int y){
    if (y0==y1) return; if (y0>y1){ int tx=x0,ty=y0; x0=x1; y0=y1; x1=tx; y1=ty; }
    if (y<y0 || y>y1) return; int dy=y1-y0; int dx=x1-x0; int num=y - y0;
    // fixed-point to avoid 64-bit division
    int x = x0 + (dx * num) / (dy?dy:1);
    if (x<minx[y]) minx[y]=x; if (x>maxx[y]) maxx[y]=x;
}

void bga_drawtri(int x0,int y0,int x1,int y1,int x2,int y2,uint32_t c){
    if(!g_lfb) return; if (y0>y1){ int tx=x0,ty=y0;x0=x1;y0=y1;x1=tx;y1=ty; } if (y0>y2){ int tx=x0,ty=y0;x0=x2;y0=y2;x2=tx;y2=ty; } if (y1>y2){ int tx=x1,ty=y1;x1=x2;y1=y2;x2=tx;y2=ty; }
    if (y0==y2) return;
    int miny=y0<0?0:y0, maxy=y2>=g_h?g_h-1:y2;
    static int minxbuf[2048], maxxbuf[2048]; if (g_h>2048) return; for(int y=miny;y<=maxy;y++){ minxbuf[y]=0x7fffffff; maxxbuf[y]=-0x7fffffff; }
    for(int y=miny;y<=maxy;y++){ tri_edge(x0,y0,x1,y1,minxbuf,maxxbuf,y); tri_edge(x1,y1,x2,y2,minxbuf,maxxbuf,y); tri_edge(x2,y2,x0,y0,minxbuf,maxxbuf,y); }
    for(int y=miny;y<=maxy;y++){ int lx=minxbuf[y], rx=maxxbuf[y]; if (lx<=rx){ if(lx<0) lx=0; if(rx>=g_w) rx=g_w-1; uint32_t* p=(uint32_t*)(g_lfb + (uint32_t)y*g_w*4u + (uint32_t)lx*4u); for(int x=lx;x<=rx;x++) *p++=c; } }
}

void bga_blit(int dx,int dy,int dw,int dh,const uint32_t* tex,int tw,int th){
    if(!g_lfb||!tex||dw<=0||dh<=0||tw<=0||th<=0) return;
    for(int y=0;y<dh;y++){
        int ty = (y*th)/dh; if ((unsigned)(dy+y)>=(unsigned)g_h) continue;
        for(int x=0;x<dw;x++){
            int tx=(x*tw)/dw; if ((unsigned)(dx+x)>=(unsigned)g_w) continue;
            uint32_t color = tex[ty*tw + tx];
            bga_drawpixel(dx+x, dy+y, color);
        }
    }
}
