#include <stdint.h>
#include <stdbool.h>

#include "../stdlibs/string.h"

void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

void int_to_string(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}

void append(char s[], char n) {
    int len = strlen(s);
    s[len] = n;
    s[len+1] = '\0';
}

bool backspace(char s[]) {
    int len = strlen(s);
    if (len > 0) {
        s[len - 1] = '\0';
        return true;
    } else {
        return false;
    }
}

void print_registers() {
    unsigned int eax_val, ebx_val, ecx_val, edx_val;
    unsigned int esi_val, edi_val, ebp_val, esp_val;
    unsigned int eflags_val;
    unsigned int int_no_val, err_code_val;
    unsigned int ds_val;
    void* eip_val;
    unsigned int cs_val;
    unsigned int useresp_val, ss_val;

    asm volatile(
        "mov %%eax, %[eax_val] \n"
        "mov %%ebx, %[ebx_val] \n"
        "mov %%ecx, %[ecx_val] \n"
        "mov %%edx, %[edx_val] \n"
        "mov %%esi, %[esi_val] \n"
        "mov %%edi, %[edi_val] \n"
        "mov %%ebp, %[ebp_val] \n"
        "mov %%esp, %[esp_val] \n"
        "mov %%cs, %[cs_val] \n"
        "pushf \n"
        "pop %[eflags_val] \n"
        "mov %%ds, %[ds_val] \n"
        "mov %%ss, %[ss_val] \n"
        : [eax_val] "=g" (eax_val),
          [ebx_val] "=g" (ebx_val),
          [ecx_val] "=g" (ecx_val),
          [edx_val] "=g" (edx_val),
          [esi_val] "=g" (esi_val),
          [edi_val] "=g" (edi_val),
          [ebp_val] "=g" (ebp_val),
          [esp_val] "=g" (esp_val),
          [eflags_val] "=g" (eflags_val),
          [ds_val] "=g" (ds_val),
          [cs_val] "=g" (cs_val),
          [ss_val] "=g" (ss_val)
        :
        : "memory"
    );

    asm volatile(
        "mov %[int_no_val], %[int_no_val] \n"
        "mov %[err_code_val], 4(%[err_code_val]) \n"
        : [int_no_val] "=&r" (int_no_val),
          [err_code_val] "=&r" (err_code_val)
    );

    asm volatile(
        "call 1f \n"
        "1: pop %[eip_val] \n"
        : [eip_val] "=g" (eip_val)
    );

    asm volatile(
        "mov %%esp, %[useresp_val] \n"
        : [useresp_val] "=g" (useresp_val)
    );

    printf("  EAX: 0x%p\n", (void*)eax_val);
    printf("  EBX: 0x%p\n", (void*)ebx_val);
    printf("  ECX: 0x%p\n", (void*)ecx_val);
    printf("  EDX: 0x%p\n", (void*)edx_val);
    printf("  ESI: 0x%p\n", (void*)esi_val);
    printf("  EDI: 0x%p\n", (void*)edi_val);
    printf("  EBP: 0x%p\n", (void*)ebp_val);
    printf("  ESP: 0x%p\n", (void*)esp_val);
    printf("  EFLAGS: 0x%p\n", (void*)eflags_val);
    printf("  DS: 0x%p\n", (void*)ds_val);
    printf("  CS: 0x%p\n", (void*)cs_val);
    printf("  SS: 0x%p\n", (void*)ss_val);
    printf("  EIP: %p\n", eip_val);
    printf("  INT_NO: 0x%p\n", (void*)int_no_val);
    printf("  ERR_CODE: 0x%p\n", (void*)err_code_val);
    printf("  USERESP: 0x%p\n", (void*)useresp_val);

    return;
}

void printascii() {
    int rows = 16;
    int columns = 16;
    printf("%c", 0xC9);
    printf("%c", 0xCD);
    printf("%c", 0xCD);
    printf("%c", 0xCD);
    printf("%c", 0xCD);
    printf("%c", 0xD1);

    for (int col = 0; col < columns<<2; col++) {
        printf("%c", 0xCD);
    }
    printf("%c\n", 0xBB);

    printf("%c", 0xBA);
    printf("    ");
    printf("%c", 0xB3);
    
    for (int col = 0; col < columns; col++) {
        printf(" ");
        printHexByte(col);
    }
    printf("%c\n", 0xBA);

    printf("%c", 0xC7);
    printf("%c", 0xC4);
    printf("%c", 0xC4);
    printf("%c", 0xC4);
    printf("%c", 0xC4);
    printf("%c", 0xC5);

    for (int col = 0; col < columns<<2; col++) {
        printf("%c", 0xC4);
    }
    printf("%c\n", 0xB6);

    for (int row = 0; row < rows; row++) {
        printf("%c ", 0xBA);
        printHexByte(row<<4);
        printf("%c", 0xB3);

        for (int col = 0; col < columns; col++) {
            unsigned char c = row * columns + col;
            if (c == 0 || c == 10) // Skip the newline character '\n'
                printf("    ");
            else
                printf("  %c ", c);
        }
        printf("%c\n", 0xBA);
    }

    printf("%c", 0xC8);
    printf("%c", 0xCD);
    printf("%c", 0xCD);
    printf("%c", 0xCD);
    printf("%c", 0xCD);
    printf("%c", 0xCF);

    for (int col = 0; col < columns<<2; col++) {
        printf("%c", 0xCD);
    }
    printf("%c\n", 0xBC);
}
