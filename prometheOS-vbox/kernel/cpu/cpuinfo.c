#include "../../include/cpu/cpuinfo.h"
#include "../../include/drivers/display.h"

void cpuinfo(void) {
    unsigned int eax, ebx, ecx, edx;
    
    // 1. Vendor ID
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    
    char vendor[13];
    *(unsigned int*)&vendor[0] = ebx;
    *(unsigned int*)&vendor[4] = edx;
    *(unsigned int*)&vendor[8] = ecx;
    vendor[12] = 0;
    
    printf("CPU Vendor: %s\n", vendor);
    
    // 2. Version Info
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    unsigned int family = (eax >> 8) & 0xF;
    unsigned int model = (eax >> 4) & 0xF;
    unsigned int stepping = eax & 0xF;
    
    printf("Family: %d Model: %d Stepping: %d\n", family, model, stepping);
}