#include "../../include/lib/string.h"
#include <stdarg.h>

// Minimale sscanf Implementierung für %d und %s
// Unterstützt aktuell nur einfache Formate wie "%d:%d" oder "%s"
int sscanf(const char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);

    int count = 0;
    const char *p = format;
    const char *s = str;

    while (*p && *s) {
        if (*p == '%') {
            p++;
            if (*p == 'd') {
                int *val = va_arg(args, int*);
                // Skip whitespace
                while (*s == ' ') s++;
                
                // Parse integer
                int res = atoi(s);
                *val = res;
                count++;
                
                // Skip digits in input
                if (*s == '-') s++;
                while (*s >= '0' && *s <= '9') s++;
            } else if (*p == 's') {
                char *buf = va_arg(args, char*);
                while (*s == ' ') s++;
                while (*s && *s != ' ' && *s != '\n') {
                    *buf++ = *s++;
                }
                *buf = '\0';
                count++;
            }
            p++;
        } else {
            // Match literal characters
            if (*s == *p) {
                s++;
                p++;
            } else {
                // Mismatch
                break;
            }
        }
    }

    va_end(args);
    return count;
}