#pragma once
#include <stdint.h>

// BGA (Bochs/QEMU VBE) – minimale 2D‑API, GL1.0‑artig
// Farbcodierung: 0x00RRGGBB (32‑bpp)

// bga_init: Schaltet BGA in w×h@32bpp; 0 = OK, <0 Fehler
int  bga_init(int w, int h);
// bga_close: Zurück in Textmodus/Disable
void bga_close(void);
// bga_clear: Bildschirm mit fester Farbe füllen
void bga_clear(uint32_t color);
// bga_drawpixel: Pixel (x,y) setzen
void bga_drawpixel(int x, int y, uint32_t color);
// bga_drawline: Bresenham‑Linie von (x0,y0) nach (x1,y1)
void bga_drawline(int x0, int y0, int x1, int y1, uint32_t color);
// bga_drawtri: Gefülltes Dreieck (Scanline)
void bga_drawtri(int x0,int y0,int x1,int y1,int x2,int y2,uint32_t color);

// Simple textured blit (nearest) into axis-aligned rect
// bga_blit: Nearest‑Blit eines ARGB32‑Textures in AABB‑Ziel
void bga_blit(int dx, int dy, int dw, int dh, const uint32_t* tex, int tw, int th);

// Query
// Query: Status und aktuelle Auflösung
int  bga_is_active(void);
int  bga_width(void);
int  bga_height(void);

// Public helper: force VGA 80x25 text mode restore (safe even if BGA inactive)
int txtmode(void);
