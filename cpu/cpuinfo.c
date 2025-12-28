#include "cpuinfo.h"
#include "../drivers/display.h"
#include "../stdlibs/string.h"
#include <stdint.h>

void cpuinfo(void) {
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];

    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = '\0';
    printf("Vendor: %s\n", vendor);

    char brand[49];
    uint32_t *brand_u32 = (uint32_t *)brand;
    for (uint32_t i = 0; i < 3; i++) {
        __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002 + i));
        brand_u32[i * 4 + 0] = eax;
        brand_u32[i * 4 + 1] = ebx;
        brand_u32[i * 4 + 2] = ecx;
        brand_u32[i * 4 + 3] = edx;
    }
    brand[48] = '\0';
    printf("Brand: %s\n", brand);

    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    printf("Stepping %d, Model %d, Family %d\n", eax & 0xf, (eax >> 4) & 0xf, (eax >> 8) & 0xf);
    printf("Features: ");
    if (edx & (1 << 4)) printf("TSC ");
    if (edx & (1 << 23)) printf("MMX ");
    if (edx & (1 << 25)) printf("SSE ");
    if (edx & (1 << 26)) printf("SSE2 ");
    if (ecx & (1 << 0)) printf("SSE3 ");
    if (ecx & (1 << 9)) printf("SSSE3 ");
    if (ecx & (1 << 19)) printf("SSE4.1 ");
    if (ecx & (1 << 20)) printf("SSE4.2 ");
    printf("\n");
}

