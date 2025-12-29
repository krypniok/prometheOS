#pragma once
#include <stddef.h>
#include "stdio_fs.h"  // FILE typedef

// Loads a PSF1/PSF2 bitmap font (first 256 glyphs only).
// Writes glyph bitmaps packed row-wise, 256 * height bytes, 1 bit per pixel (MSB first, width truncated to 8px).
// Returns 1 on success, 0 on failure.
int load_psf_font(FILE* fp, unsigned char* out_glyphs, size_t* out_height);

