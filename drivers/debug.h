#pragma once
#include <stdint.h>

#ifdef ENABLE_DEBUG
void debug_putc(char c);
void debug_puts(const char *s);
void debug_hex(uint32_t v);
void quit_qemu(void);
#else
static inline void debug_putc(char c) { (void)c; }
static inline void debug_puts(const char *s) { (void)s; }
static inline void debug_hex(uint32_t v) { (void)v; }
static inline void quit_qemu(void) { (void)0; }
#endif
