#include <stdint.h>
#include <stddef.h>
#include "stdio_fs.h"  // maps FILE/fopen/fread/fclose to DB backend
#include "memory.h"
#include "bmp.h"

typedef struct __attribute__((packed)) {
    uint16_t bfType;      // 'BM'
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;   // offset to pixel data
} BITMAPFILEHEADER;

typedef struct __attribute__((packed)) {
    uint32_t biSize;      // 40
    int32_t  biWidth;
    int32_t  biHeight;    // positive: bottom-up
    uint16_t biPlanes;    // 1
    uint16_t biBitCount;  // 24 or 32
    uint32_t biCompression; // 0 = BI_RGB
    uint32_t biSizeImage;   // can be 0 for BI_RGB
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;

static int fread_all(void* dst, size_t n, FILE* f){ return fread(dst, 1, n, f) == n; }
static int fseek_skip(FILE* f, size_t n){
    // no fseek: read-and-discard
    const size_t chunk = 256;
    unsigned char drop[chunk];
    size_t left = n;
    while (left){ size_t r = left > chunk ? chunk : left; if (fread(drop,1,r,f)!=r) return 0; left -= r; }
    return 1;
}

int load_bmp(FILE* fp, Bmp32* out){
    if (!fp || !out) return 0;
    BITMAPFILEHEADER bfh; BITMAPINFOHEADER bih;
    if (!fread_all(&bfh, sizeof(bfh), fp)) return 0;
    if (bfh.bfType != 0x4D42) return 0; // 'BM'
    if (!fread_all(&bih, sizeof(bih), fp)) return 0;
    // Accept BI_RGB (0) for 24/32bpp and BI_BITFIELDS (3) for many 32bpp BMPs
    int bpp_ok = (bih.biBitCount == 24) || (bih.biBitCount == 32);
    int comp_ok = (bih.biCompression == 0) || (bih.biCompression == 3 && bih.biBitCount == 32);
    if (!bpp_ok || !comp_ok) return 0;
    int w = bih.biWidth; int h = bih.biHeight < 0 ? -bih.biHeight : bih.biHeight; int bpp = bih.biBitCount;
    int bottom_up = (bih.biHeight > 0);

    // jump to pixel data
    size_t header_read = sizeof(bfh) + sizeof(bih);
    if (bfh.bfOffBits > header_read){ if (!fseek_skip(fp, bfh.bfOffBits - header_read)) return 0; }

    size_t out_bytes = (size_t)w * (size_t)h * 4;
    uint8_t* pix = (uint8_t*)malloc(out_bytes);
    if (!pix) return 0;

    if (bpp == 32){
        // Many BMPs store 32bpp as BGRA little-endian. Convert to RGBA with A preserved.
        size_t row_in = (size_t)w * 4;
        uint8_t* row = (uint8_t*)malloc(row_in);
        if (!row){ free(pix); return 0; }
        for (int y = 0; y < h; y++){
            int dst_y = bottom_up ? (h - 1 - y) : y;
            if (!fread_all(row, row_in, fp)) { free(row); free(pix); return 0; }
            uint8_t* d = (uint8_t*)(pix + (size_t)dst_y * w * 4);
            for (int x = 0; x < w; x++){
                uint8_t b = row[x*4+0];
                uint8_t g = row[x*4+1];
                uint8_t r = row[x*4+2];
                uint8_t a = row[x*4+3];
                d[x*4+0] = r;
                d[x*4+1] = g;
                d[x*4+2] = b;
                d[x*4+3] = a;
            }
        }
        free(row);
    } else { // 24 bpp
        size_t row_in = ((size_t)w * 3 + 3) & ~3u; // padded to 4 bytes
        uint8_t* row = (uint8_t*)malloc(row_in);
        if (!row){ free(pix); return 0; }
        for (int y = 0; y < h; y++){
            int dst_y = bottom_up ? (h - 1 - y) : y;
            if (!fread_all(row, row_in, fp)) { free(row); free(pix); return 0; }
            uint8_t* d = (uint8_t*)(pix + (size_t)dst_y * w * 4);
            for (int x = 0; x < w; x++){
                uint8_t b = row[x*3+0];
                uint8_t g = row[x*3+1];
                uint8_t r = row[x*3+2];
                d[x*4+0] = r;
                d[x*4+1] = g;
                d[x*4+2] = b;
                d[x*4+3] = 0xFF;
            }
        }
        free(row);
    }

    out->pixels = pix; out->width = w; out->height = h;
    return 1;
}

void free_bmp(Bmp32* bmp){ if (bmp && bmp->pixels) { free(bmp->pixels); bmp->pixels = 0; } }
