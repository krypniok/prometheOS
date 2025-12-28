#ifndef STRING_H
#define STRING_H

int strcmp(const char *s1, const char *s2);
int atoi(const char *s);
int strlen(const char *s);
char *strcpy(char *dest, const char *src);
void *memcpy(void *dest, const void *src, int n);
void *memset(void *s, int c, int n);

#endif