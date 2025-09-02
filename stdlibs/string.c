#include "string.h"

char tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

bool isalnum(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool isdigit(char c) {
    return c >= '0' && c <= '9';
}

size_t strlen(const char* str) {
    const char* ptr = str;
    while (*ptr)
        ptr++;
    return ptr - str;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

int atoi(const char *str) {
    int result = 0;
    int sign = 1;
    int i = 0;

    // Handling whitespaces
    while (str[i] == ' ') {
        i++;
    }

    // Handling sign
    if (str[i] == '+' || str[i] == '-') {
        sign = (str[i++] == '-') ? -1 : 1;
    }

    // Converting digits to integer
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return sign * result;
}

unsigned long strtoul(const char* str, char** endptr, int base) {
    unsigned long result = 0;
    bool isNegative = false;

    // Überspringe führende Leerzeichen
    while (*str == ' ')
        str++;

    // Überprüfe auf Vorzeichen
    if (*str == '-') {
        isNegative = true;
        str++;
    } else if (*str == '+') {
        str++;
    }

    // Überprüfe auf Basis 0x für Hexadezimal
    if (base == 0 && *str == '0') {
        str++;
        if (*str == 'x' || *str == 'X') {
            base = 16;
            str++;
        } else {
            base = 8;
        }
    }

    // Überprüfe auf Basis 0 für Oktal
    if (base == 0 && *str == '0') {
        base = 8;
        str++;
    }

    // Wenn die Basis immer noch 0 ist, setzen wir sie auf 10
    if (base == 0)
        base = 10;

    // Konvertiere die Zeichenkette in eine Ganzzahl
    while (*str != '\0') {
        int digit;
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'A' && *str <= 'Z') {
            digit = *str - 'A' + 10;
        } else if (*str >= 'a' && *str <= 'z') {
            digit = *str - 'a' + 10;
        } else {
            break; // Ungültiges Zeichen, beende die Konvertierung
        }

        if (digit >= base)
            break; // Ungültiges Zeichen für die gegebene Basis, beende die Konvertierung

        result = result * base + digit;
        str++;
    }

    // Setze den Zeiger auf das nächste Zeichen nach der konvertierten Zahl
    if (endptr != NULL)
        *endptr = (char*)str;

    // Setze das Vorzeichen entsprechend
    if (isNegative)
        result = -result;

    return result;
}

char* strstr(const char* haystack, const char* needle) {
    if (*needle == '\0') {
        return (char*)haystack;
    }

    while (*haystack != '\0') {
        const char* h = haystack;
        const char* n = needle;
        while (*h == *n && *n != '\0') {
            h++;
            n++;
        }
        if (*n == '\0') {
            return (char*)haystack;
        }
        haystack++;
    }

    return NULL;
}

char* strchr(const char* str, int ch) {
    while (*str != '\0') {
        if (*str == ch) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

char* strtok(char* str, const char* delim) {
    static char* savedStr = NULL;
    if (str != NULL) {
        savedStr = str;
    }
    if (savedStr == NULL || *savedStr == '\0') {
        return NULL;
    }
    char* token = savedStr;
    while (*savedStr != '\0') {
        if (strchr(delim, *savedStr) != NULL) {
            *savedStr = '\0';
            savedStr++;
            return token;
        }
        savedStr++;
    }
    return token;
}

// Helper function to search for a byte within a memory range
void* search_byte(void* start_address, size_t size, unsigned char byteToFind) {
    for (size_t i = 0; i < size; i++) {
        unsigned char byte = *((unsigned char*)(start_address + i));
        if (byte == byteToFind) {
            // Byte found, return the address
            return (start_address + i);
        }
    }

    // Byte not found in the given range
    return NULL;
}

// Helper function to search for a string within a memory range
void* search_string(void* start_address, size_t size, const char* str) {
    size_t str_len = strlen(str);

    for (size_t i = 0; i < size - str_len + 1; i++) {
        if (memcmp(start_address + i, str, str_len) == 0) {
            // String found, return the address
            return start_address + i;
        }
    }

    // String not found in the given range
    return NULL;
}

char* strdup(const char *str) {
    size_t len = strlen(str) + 1;
    char *copy = (char *)malloc(len);
    if (copy) {
        strcpy(copy, str);
    }
    return copy;
}

char* strcpy(char *dest, const char *src) {
    char *original_dest = dest;
    while (*src) {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return original_dest;
}

char* strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    // Falls n größer als die Länge von src ist, fülle den Rest von dest mit null Bytes
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char * strcat(char *dest, const char *src) {
    char *original_dest = dest;
    while (*dest) {
        dest++;
    }
    while (*src) {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return original_dest;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *original_dest = dest;

    // Gehe zum Ende von dest
    while (*dest != '\0') {
        dest++;
    }

    // Kopiere die ersten 'n' Zeichen von src an das Ende von dest
    while (*src != '\0' && n > 0) {
        *dest = *src;
        dest++;
        src++;
        n--;
    }

    // Füge das Abschlusszeichen hinzu
    *dest = '\0';

    return original_dest;
}
