#include <stddef.h>
#include <stdbool.h>

char* strdup(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);

char* strstr(const char* haystack, const char* needle);
char* strchr(const char* str, int ch);
char* strtok(char* str, const char* delim);
int strcmp(const char* str1, const char* str2);
int strncmp(const char *s1, const char *s2, size_t n);

size_t strlen(const char* str);

int atoi(const char *str);
unsigned long strtoul(const char* str, char** endptr, int base);

void* search_byte(void* start_address, size_t size, unsigned char byteToFind);
void* search_string(void* start_address, size_t size, const char* str);

char tolower(char c);
bool isalnum(char c);
bool isdigit(char c);
