#include "font_psf.h"
#include "memory.h"

#pragma pack(push,1)
typedef struct { unsigned char magic[2]; unsigned char mode; unsigned char charsize; } PSF1Header;
typedef struct { unsigned int magic; unsigned int version; unsigned int headersize; unsigned int flags; unsigned int length; unsigned int charsize; unsigned int height; unsigned int width; } PSF2Header;
#pragma pack(pop)

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04
#define PSF2_MAGIC  0x864ab572u

int load_psf_font(FILE* fp, unsigned char* out_glyphs, size_t* out_height){
    if (!fp || !out_glyphs || !out_height) return 0;
    unsigned char hdrbuf[64];
    long pos = 0;
    size_t n = fread(hdrbuf, 1, sizeof(hdrbuf), fp);
    if (n < 4) return 0;
    // PSF2?
    const PSF2Header* h2 = (const PSF2Header*)hdrbuf;
    const PSF1Header* h1 = (const PSF1Header*)hdrbuf;

    // rewind to start
    // Simple FILE impl lacks fseek; reopen and skip manually
    // In our stdio_fs, fread advances pos; easiest is to close/reopen
    // But we can just parse size/offset and read again from start by reopening.
    // Instead, assume tsqlfs supports reopen, so user should pass a fresh fp.

    // Try PSF2
    if (n >= sizeof(PSF2Header) && h2->magic == PSF2_MAGIC && h2->headersize >= sizeof(PSF2Header)){
        unsigned int length = h2->length ? h2->length : 256;
        unsigned int charsize = h2->charsize;
        unsigned int height = h2->height ? h2->height : (charsize ? charsize : 16);
        unsigned int width  = h2->width ? h2->width : 8;
        if (length < 256) return 0;
        if (height == 0 || height > 32) height = (height==0)?16:32;
        *out_height = height;

        // Compute glyphs data start
        // We already read headersize bytes into hdrbuf; we need to reconstruct the stream.
        // Simplify: re-open the file and skip to headersize.
        // Our FILE impl does not support seek; workaround: close and reopen outside is cumbersome.
        // So we require that the caller opens a fresh FILE here. Since we can't seek, we will read and discard headersize bytes now.
        // We already consumed n bytes; read and discard (headersize - n) more if needed.
        unsigned int off = h2->headersize;
        if (off > n){
            unsigned int to_skip = off - (unsigned int)n;
            unsigned char dump[64];
            while (to_skip){ size_t k = to_skip > sizeof(dump) ? sizeof(dump) : to_skip; size_t r = fread(dump,1,k,fp); if (!r) break; to_skip -= (unsigned int)r; }
        }

        // Read first 256 glyphs; PSF2 stores glyphs row-packed, each row padded to byte boundary of width
        unsigned int rowbytes = (width + 7) / 8;
        for (unsigned int g = 0; g < 256; g++){
            for (unsigned int r = 0; r < height; r++){
                unsigned char row[8];
                if (fread(row,1,rowbytes,fp) != rowbytes) return 0;
                // Truncate/pack to 1 byte (first 8 pixels)
                out_glyphs[g*height + r] = row[0];
            }
        }
        return 1;
    }

    // Try PSF1
    if (hdrbuf[0] == PSF1_MAGIC0 && hdrbuf[1] == PSF1_MAGIC1){
        unsigned int charsize = h1->charsize ? h1->charsize : 16;
        if (charsize == 0 || charsize > 32) charsize = 16;
        *out_height = charsize;

        // PSF1 has 256 or 512 glyphs; we only need 256
        unsigned int glyphs = (h1->mode & 0x01) ? 512 : 256;
        unsigned int need = charsize * glyphs;

        // Reopen workaround: we already consumed sizeof(PSF1Header) bytes; proceed with remainder
        // Read first 256 glyphs
        for (unsigned int g=0; g<256; g++){
            if (fread(out_glyphs + g*charsize, 1, charsize, fp) != charsize) return 0;
        }
        // If 512 glyphs, skip the rest
        unsigned int to_skip = (glyphs-256)*charsize;
        unsigned char dump[64];
        while (to_skip){ size_t k = to_skip > sizeof(dump) ? sizeof(dump) : to_skip; size_t r = fread(dump,1,k,fp); if (!r) break; to_skip -= (unsigned int)r; }
        return 1;
    }

    return 0;
}

