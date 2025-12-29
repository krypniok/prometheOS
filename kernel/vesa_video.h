#pragma once
#include <stdint.h>
#include <stddef.h>

// VESA VBE Mode Info Block structure
typedef struct {
    uint16_t attributes;
    uint8_t  winA, winB;
    uint16_t granularity;
    uint16_t winsize;
    uint16_t segmentA, segmentB;
    uint32_t realFctPtr;
    uint16_t pitch;
    uint16_t Xres, Yres;
    uint8_t  Wchar, Ychar, planes, bpp, banks;
    uint8_t  memory_model, bank_size, image_pages;
    uint8_t  reserved0;
    uint8_t  red_mask, red_position;
    uint8_t  green_mask, green_position;
    uint8_t  blue_mask, blue_position;
    uint8_t  rsv_mask, rsv_position;
    uint8_t  directcolor_attributes;
    uint32_t physbase; // LFB address
    uint32_t reserved1;
    uint16_t reserved2;
} __attribute__((packed)) VbeModeInfoBlock;


// VESA graphics functions (repurposed from BGA names)
// Farbcodierung: 0x00RRGGBB (32‑bpp)

// vesa_init: Reads VBE info passed by bootloader. 0 = OK, <0 Fehler
int  bga_init(int w, int h); // w and h are ignored, but kept for compatibility
// vesa_close: Zurück in Textmodus/Disable
void bga_close(void);
// vesa_clear: Bildschirm mit fester Farbe füllen
void bga_clear(uint32_t color);
// vesa_drawpixel: Pixel (x,y) setzen
void bga_drawpixel(int x, int y, uint32_t color);
// vesa_drawline: Bresenham‑Linie von (x0,y0) nach (x1,y1)
void bga_drawline(int x0, int y0, int x1, int y1, uint32_t color);
// vesa_drawtri: Gefülltes Dreieck (Scanline)
void bga_drawtri(int x0,int y0,int x1,int y1,int x2,int y2,uint32_t color);

// Simple textured blit (nearest) into axis-aligned rect
void bga_blit(int dx, int dy, int dw, int dh, const uint32_t* tex, int tw, int th);

// Query: Status und aktuelle Auflösung
int  bga_is_active(void);
int  bga_width(void);
int  bga_height(void);

// Snapshot helpers: capture or restore raw LFB contents
size_t bga_snapshot_size(void);
int  bga_snapshot(void* dest, size_t bytes);
int  bga_restore_from_snapshot(int w, int h, const void* src, size_t bytes);

// Public helper: force VGA 80x25 text mode restore
void vga_diag_txtforce(void);
int txtmode(void);
