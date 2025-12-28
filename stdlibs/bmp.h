#pragma once

#include <stddef.h>
#include <stdint.h>

// Load a BMP (24/32 bpp) from stdio-like FILE into 32-bpp RGBA buffer.
// Returns 1 on success, 0 on failure. Allocates pixels with malloc; caller frees.
// Pixels are bottom-up converted to top-down, premultiplied alpha is NOT applied.
typedef struct {
    uint8_t* pixels; // 32-bpp, RGBA (A=0xFF for 24-bpp inputs)
    int width;
    int height;
} Bmp32;

int load_bmp(FILE* fp, Bmp32* out);
void free_bmp(Bmp32* bmp);

