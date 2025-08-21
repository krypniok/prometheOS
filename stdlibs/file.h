#define MAX_FILES 100
#define MAX_FILE_SIZE 4096

typedef struct {
    char filename[32];
    char content[MAX_FILE_SIZE];
    size_t size;
    int mode;
} RAMFILEOLD;

typedef struct {
    char filename[32];
    char *content;
    size_t size;
    char mode;
} RAMFILE;

RAMFILE* ramdisk_fopen(const char *filename, const char *mode);
size_t ramdisk_fread(void *ptr, size_t size, size_t count, RAMFILE* stream);
size_t ramdisk_fwrite(const void *ptr, size_t size, size_t count, RAMFILE* stream);

int ramdisk_fclose(RAMFILE* stream);

int ramdisk_test();
