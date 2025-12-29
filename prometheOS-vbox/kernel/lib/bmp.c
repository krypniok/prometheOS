#include "../../include/lib/bmp.h"
#include "../../include/lib/stdio_fs.h"
#include "../../include/lib/memory.h"
#include "../../include/lib/string.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPHeader;

typedef struct {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_res;
    int32_t  y_res;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPInfoHeader;
#pragma pack(pop)

int load_bmp(FILE* fp, Bmp32* out) {
    if (!fp || !out) return 0;

    BMPHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) return 0;

    if (header.type != 0x4D42) return 0; // 'BM'

    BMPInfoHeader info;
    if (fread(&info, sizeof(info), 1, fp) != 1) return 0;

    if (info.bpp != 24 && info.bpp != 32) return 0; // Only support 24/32 bpp for now
    if (info.compression != 0) return 0; // No compression

    out->width = info.width;
    out->height = (info.height > 0) ? info.height : -info.height;
    int row_padded = (info.width * (info.bpp / 8) + 3) & (~3);
    
    out->pixels = (uint8_t*)malloc(out->width * out->height * 4);
    if (!out->pixels) return 0;

    fseek(fp, header.offset, SEEK_SET);

    uint8_t* row_data = (uint8_t*)malloc(row_padded);
    if (!row_data) { free(out->pixels); return 0; }

    for (int y = 0; y < out->height; y++) {
        if (fread(row_data, 1, row_padded, fp) != (size_t)row_padded) {
            free(row_data);
            free(out->pixels);
            return 0;
        }

        // BMP is usually bottom-up
        int target_y = (info.height > 0) ? (out->height - 1 - y) : y;
        uint8_t* dest_row = out->pixels + (target_y * out->width * 4);

        for (int x = 0; x < out->width; x++) {
            int src_idx = x * (info.bpp / 8);
            int dst_idx = x * 4;
            
            uint8_t b = row_data[src_idx + 0];
            uint8_t g = row_data[src_idx + 1];
            uint8_t r = row_data[src_idx + 2];
            uint8_t a = (info.bpp == 32) ? row_data[src_idx + 3] : 255;

            dest_row[dst_idx + 0] = r;
            dest_row[dst_idx + 1] = g;
            dest_row[dst_idx + 2] = b;
            dest_row[dst_idx + 3] = a;
        }
    }

    free(row_data);
    return 1;
}

void free_bmp(Bmp32* bmp) {
    if (bmp && bmp->pixels) {
        free(bmp->pixels);
        bmp->pixels = NULL;
    }
}