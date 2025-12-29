#include <stdint.h>
#include "../drivers/ports.h"
#include "vesa_video.h"
#include "../drivers/display.h" // for VGA text console helpers
#include "../stdlibs/string.h"

// Pointer to the VBE info block passed by the bootloader
#define VBE_INFO_BLOCK_ADDR ((VbeModeInfoBlock*)0x9000)

// Globals for video state
static uint32_t g_lfb = 0; // Linear Framebuffer Address
static int g_w = 0, g_h = 0, g_pitch = 0, g_bpp = 0;

// Saved text-mode state
static uint8_t g_saved_font[256*32];
static size_t  g_saved_font_h = 16;
static int     g_saved_cursor = 0;

// vesa_init: Reads VBE info passed by bootloader.
int bga_init(int w, int h) {
    (void)w; (void)h; // Ignored, we use what the bootloader set

    VbeModeInfoBlock* vbe_info = VBE_INFO_BLOCK_ADDR;

    // Check if the bootloader successfully set a mode
    // A valid LFB address is the main indicator.
    if (vbe_info->physbase == 0) {
        return -1; // No LFB, no graphics mode
    }

    // Check for a usable mode (32-bit color, linear framebuffer)
    if (vbe_info->bpp != 32 || !(vbe_info->attributes & (1 << 7))) {
        return -1;
    }

    g_lfb = vbe_info->physbase;
    g_w = vbe_info->Xres;
    g_h = vbe_info->Yres;
    g_pitch = vbe_info->pitch;
    g_bpp = vbe_info->bpp;

    // Capture current text-mode font for later restoration
    g_saved_font_h = vga_get_font_height();
    if (g_saved_font_h == 0 || g_saved_font_h > 32) g_saved_font_h = 16;
    g_saved_cursor = get_cursor();
    save_vga_font(g_saved_font, g_saved_font_h);

    return 0;
}

// Minimal VGA text-mode restore (80x25, default 16-color DAC)
static void _vga_set_text_mode_80x25(void){
    // This is a standard VGA register sequence to force 80x25 text mode
    static const unsigned char regs[] = {
        0x67,0x00,0x03,0x00,0x07,0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x50,
        0x9C,0x0E,0x8F,0x28,0x40,0x96,0xB9,0xA3,0xFF,0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x00,0xFF,
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
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
    (void)port_byte_in(0x3DA);
    port_byte_out(0x3C0, 0x30); port_byte_out(0x3C0, 0x41);
    (void)port_byte_in(0x3DA);
    port_byte_out(0x3C0, 0x33); port_byte_out(0x3C0, 0x00);
    for (int i=0;i<16;i++){ (void)port_byte_in(0x3DA); port_byte_out(0x3C0, i); port_byte_out(0x3C0, i); }
    (void)port_byte_in(0x3DA);
    port_byte_out(0x3C0, 0x20);
}

void bga_close(void) {
    if (!g_lfb) return; // Not in a graphics mode
    _vga_set_text_mode_80x25();
    // Restore font, DAC, and console state
    load_vga_font(g_saved_font, g_saved_font_h);
    console_page_end();
    set_color(WHITE_ON_BLACK);
    clear_screen();
    console_flush_to_vga();
    set_cursor(g_saved_cursor);
    g_lfb = 0; g_w = 0; g_h = 0; g_pitch = 0;
}

int txtmode(void) {
    bga_close();
    return 0;
}

// Public helper to force text mode
void vga_diag_txtforce(void) {
    bga_close();
}

int bga_is_active(void) { return g_lfb != 0; }
int bga_width(void) { return g_w; }
int bga_height(void) { return g_h; }

void bga_drawpixel(int x, int y, uint32_t c) {
    if (!g_lfb || (unsigned)x >= (unsigned)g_w || (unsigned)y >= (unsigned)g_h) {
        return;
    }
    volatile uint8_t* fb_line = (uint8_t*)(g_lfb + y * g_pitch);
    volatile uint32_t* fb_pixel = (uint32_t*)(fb_line + x * (g_bpp / 8));
    *fb_pixel = c;
}

void bga_clear(uint32_t color) {
    if (!g_lfb) return;
    // This is slow; a memset or optimized version would be better.
    for (int y = 0; y < g_h; y++) {
        for (int x = 0; x < g_w; x++) {
            bga_drawpixel(x, y, color);
        }
    }
}

void bga_drawline(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1), sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    while (1) {
        bga_drawpixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Snapshot and drawing functions that depend on a framebuffer
// These are simplified or adapted from the original BGA code

size_t bga_snapshot_size(void) {
    if (!g_lfb) return 0;
    return (size_t)g_h * (size_t)g_pitch;
}

int bga_snapshot(void* dest, size_t bytes) {
    size_t need = bga_snapshot_size();
    if (!need || !dest || bytes < need) return -1;
    memcpy(dest, (const void*)g_lfb, need);
    return 0;
}

int bga_restore_from_snapshot(int w, int h, const void* src, size_t bytes) {
    if (!g_lfb || !src || w != g_w || h != g_h) return -1;
    size_t need = bga_snapshot_size();
    if (bytes < need) return -1;
    memcpy((void*)g_lfb, src, need);
    return 0;
}

static void tri_edge(int x0, int y0, int x1, int y1, int* minx, int* maxx, int y) {
    if (y0 == y1) return;
    if (y0 > y1) { int tx = x0, ty = y0; x0 = x1; y0 = y1; x1 = tx; y1 = ty; }
    if (y < y0 || y > y1) return;
    int dy = y1 - y0;
    int dx = x1 - x0;
    int x = x0 + (dx * (y - y0)) / (dy ? dy : 1);
    if (x < minx[y]) minx[y] = x;
    if (x > maxx[y]) maxx[y] = x;
}

void bga_drawtri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
    if (!g_lfb) return;
    if (y0 > y1) { int tx = x0, ty = y0; x0 = x1; y0 = y1; x1 = tx; y1 = ty; }
    if (y0 > y2) { int tx = x0, ty = y0; x0 = x2; y0 = y2; x2 = tx; y2 = ty; }
    if (y1 > y2) { int tx = x1, ty = y1; x1 = x2; y1 = y2; x2 = tx; y2 = ty; }
    if (y0 == y2) return;
    int miny = y0 < 0 ? 0 : y0;
    int maxy = y2 >= g_h ? g_h - 1 : y2;
    static int minxbuf[2048], maxxbuf[2048];
    if (g_h > 2048) return; // Failsafe
    for (int y = miny; y <= maxy; y++) { minxbuf[y] = 0x7FFFFFFF; maxxbuf[y] = -0x7FFFFFFF; }
    for (int y = miny; y <= maxy; y++) {
        tri_edge(x0, y0, x1, y1, minxbuf, maxxbuf, y);
        tri_edge(x1, y1, x2, y2, minxbuf, maxxbuf, y);
        tri_edge(x2, y2, x0, y0, minxbuf, maxxbuf, y);
    }
    for (int y = miny; y <= maxy; y++) {
        int lx = minxbuf[y], rx = maxxbuf[y];
        if (lx <= rx) {
            if (lx < 0) lx = 0;
            if (rx >= g_w) rx = g_w - 1;
            uint8_t* p_line = (uint8_t*)(g_lfb + y * g_pitch);
            uint32_t* p = (uint32_t*)(p_line + lx * (g_bpp / 8));
            for (int x = lx; x <= rx; x++) {
                *p++ = c;
            }
        }
    }
}

void bga_blit(int dx, int dy, int dw, int dh, const uint32_t* tex, int tw, int th) {
    if (!g_lfb || !tex || dw <= 0 || dh <= 0 || tw <= 0 || th <= 0) return;
    for (int y = 0; y < dh; y++) {
        if ((unsigned)(dy + y) >= (unsigned)g_h) continue;
        int ty = (y * th) / dh;
        for (int x = 0; x < dw; x++) {
            if ((unsigned)(dx + x) >= (unsigned)g_w) continue;
            int tx = (x * tw) / dw;
            uint32_t color = tex[ty * tw + tx];
            bga_drawpixel(dx + x, dy + y, color);
        }
    }
}
