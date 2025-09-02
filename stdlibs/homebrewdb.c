#include <stddef.h>

#include "file.h"
#include "string.h"
#include "memory.h"
#include "homebrewdb.h"
#include "../drivers/hdd.h"


#define MAX_BLOB_SIZE 1024*1024 // 1 MB
#define MAX_STRING_LENGTH 256
#define MAX_COLUMNS 10

#define FILE RAMFILE
#define fread ramdisk_fread
#define fwrite ramdisk_fwrite
#define fopen ramdisk_fopen
#define fclose ramdisk_fclose


typedef enum {
    INTEGER,
    STRING,
    BLOB
} DataType;

typedef struct {
    char *name;
    void ***data;
    size_t *data_sizes; // New data structure to store BLOB sizes
    DataType *data_types;
    int num_rows;
    int num_columns;
    int max_rows;
} Table;

typedef struct {
    Table *tables;
    int num_tables;
} Database;

Database database;

static unsigned int g_hbdb_image_lba = 2048; // 1 MiB
static int g_hbdb_autosave = 0;
static unsigned int g_hbdb_max_bytes = 32 * 1024 * 1024; // default 32 MiB cap for DB payload

void create_table(char *table_name, int num_columns, DataType data_types[]) {
    if (database.num_tables == 0) {
        database.tables = (Table *)malloc(sizeof(Table));
    } else {
        database.tables = (Table *)realloc(database.tables, (database.num_tables + 1) * sizeof(Table));
    }

    Table *new_table = &(database.tables[database.num_tables]);
    new_table->name = strdup(table_name);
    new_table->data = (void ***)malloc(2 * sizeof(void **)); // Start with space for 2 rows
    new_table->data_sizes = (size_t *)malloc(2 * sizeof(size_t)); // Track BLOB sizes per row
    new_table->data_types = (DataType *)malloc(num_columns * sizeof(DataType));
    new_table->num_rows = 0;
    new_table->num_columns = num_columns;
    new_table->max_rows = 2;

    for (int i = 0; i < 2; i++) {
        new_table->data[i] = (void **)malloc(num_columns * sizeof(void *));
    }

    memcpy(new_table->data_types, data_types, num_columns * sizeof(DataType));

    database.num_tables++;
}

void insert_into_table(char *table_name, void *values[], size_t data_sizes[]) {
    Table *table = NULL;
    for (int i = 0; i < database.num_tables; i++) {
        if (strcmp(database.tables[i].name, table_name) == 0) {
            table = &(database.tables[i]);
            break;
        }
    }

    if (!table) {
        printf("Table '%s' not found!\n", table_name);
        return;
    }

    if (table->num_rows >= table->max_rows) {
        // Double the size of the table if it's full
        int new_max_rows = table->max_rows * 2;
        table->data = (void ***)realloc(table->data, new_max_rows * sizeof(void **));

        for (int i = table->max_rows; i < new_max_rows; i++) {
            table->data[i] = (void **)malloc(table->num_columns * sizeof(void *));
        }

        table->data_sizes = (size_t *)realloc(table->data_sizes, new_max_rows * sizeof(size_t));
        table->max_rows = new_max_rows;
    }

    for (int i = 0; i < table->num_columns; i++) {
        switch (table->data_types[i]) {
            case INTEGER:
                table->data[table->num_rows][i] = malloc(sizeof(int));
                *(int *)table->data[table->num_rows][i] = *(int *)values[i];
                break;
            case STRING:
                table->data[table->num_rows][i] = strdup((char *)values[i]);
                break;
            case BLOB: {
                size_t blob_size = data_sizes[i];
                if (blob_size > MAX_BLOB_SIZE) {
                    printf("Blob size exceeds the maximum limit for column %d in table %s\n", i, table_name);
                    return;
                }
                table->data[table->num_rows][i] = malloc(blob_size);
                memcpy(table->data[table->num_rows][i], values[i], blob_size);
                table->data_sizes[table->num_rows] = blob_size; // Store the size of the BLOB
            } break;
            default:
                printf("Invalid data type for column %d in table %s\n", i, table_name);
                return;
        }
    }

    table->num_rows++;
}

void select_from_table(char *table_name) {
    Table *table = NULL;
    for (int i = 0; i < database.num_tables; i++) {
        printf("%p vs.", database.tables[i].name);
        printf("%s\n", table_name);
        if (strcmp(database.tables[i].name, table_name) == 0) {

            table = &(database.tables[i]);
            break;
      }
    }

    if (!table) {
        printf("Table '%s' not found!\n", table_name);
        return;
    }

    printf("-- Table: %s --\n", table_name);
    for (int i = 0; i < table->num_rows; i++) {
        for (int j = 0; j < table->num_columns; j++) {
            switch (table->data_types[j]) {
                case INTEGER:
                    printf("%d ", *(int *)table->data[i][j]);
                    break;
                case STRING:
                    printf("%s ", (char *)table->data[i][j]);
                    break;
                case BLOB: {
                    unsigned char *blob_data = (unsigned char *)table->data[i][j];
                    size_t blob_size = table->data_sizes[i]; // Retrieve the size of the BLOB from the new data structure
                    for (int k = 0; k < blob_size; k++) {
                        printHexByte(blob_data[k]);
                    }
                } break;
            }
        }
        printf("\n");
    }
}

void free_table(Table *table) {
    for (int i = 0; i < table->num_rows; i++) {
        for (int j = 0; j < table->num_columns; j++) {
            if (table->data_types[j] == STRING || table->data_types[j] == BLOB) {
                free(table->data[i][j]);
            }
        }
        free(table->data[i]);
    }
    free(table->data);
    free(table->data_sizes); // Free memory for BLOB sizes
    free(table->data_types);
    free(table->name);
}

void free_database() {
    for (int i = 0; i < database.num_tables; i++) {
        free_table(&(database.tables[i]));
    }
    if (database.tables) {
        free(database.tables);
    }
    // Reset global state so subsequent users can safely re-init
    database.tables = NULL;
    database.num_tables = 0;
}

void save_database(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Fehler beim Öffnen der Datei '%s' zum Schreiben!\n", filename);
        return;
    }

    fwrite(&database.num_tables, sizeof(int), 1, file);

    for (int i = 0; i < database.num_tables; i++) {
        Table *table = &(database.tables[i]);

        // Tabellennamenlänge und -name speichern
        int name_length = strlen(table->name);
        fwrite(&name_length, sizeof(int), 1, file);
        fwrite(table->name, sizeof(char), name_length, file);

        // Andere Tabelleninformationen speichern (num_columns, data_types, num_rows usw.)
        fwrite(&table->num_columns, sizeof(int), 1, file);
        fwrite(table->data_types, sizeof(DataType), table->num_columns, file);
        fwrite(&table->num_rows, sizeof(int), 1, file);

        for (int j = 0; j < table->num_rows; j++) {
            for (int k = 0; k < table->num_columns; k++) {
                switch (table->data_types[k]) {
                    case INTEGER:
                        fwrite(table->data[j][k], sizeof(int), 1, file);
                        break;
                    case STRING: {
                        int string_length = strlen((char *)table->data[j][k]);
                        fwrite(&string_length, sizeof(int), 1, file);
                        fwrite(table->data[j][k], sizeof(char), string_length, file);
                    } break;
                    case BLOB: {
                        size_t blob_size = table->data_sizes[j];
                        fwrite(&blob_size, sizeof(size_t), 1, file);
                        fwrite(table->data[j][k], sizeof(unsigned char), blob_size, file);
                    } break;
                }
            }
        }
    }

    fclose(file);
}

void load_database(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Fehler beim Öffnen der Datei '%s' zum Lesen!\n", filename);
        return;
    }

    free_database(); // Vorhandene Datenbank freigeben, wenn vorhanden

    fread(&database.num_tables, sizeof(int), 1, file);
    database.tables = (Table *)malloc(database.num_tables * sizeof(Table));

    for (int i = 0; i < database.num_tables; i++) {
        Table *table = &(database.tables[i]);

        // Tabellennamenlänge und -name lesen
        int name_length;
        fread(&name_length, sizeof(int), 1, file);
        table->name = (char *)malloc((name_length + 1) * sizeof(char));
        fread(table->name, sizeof(char), name_length, file);
        table->name[name_length] = '\0'; // Tabellennamen null-terminieren

        // Andere Tabelleninformationen lesen (num_columns, data_types, num_rows usw.)
        fread(&table->num_columns, sizeof(int), 1, file);
        table->data_types = (DataType *)malloc(table->num_columns * sizeof(DataType));
        fread(table->data_types, sizeof(DataType), table->num_columns, file);
        fread(&table->num_rows, sizeof(int), 1, file);

        table->max_rows = table->num_rows * 2;
        table->data = (void ***)malloc(table->max_rows * sizeof(void **));
        table->data_sizes = (size_t *)malloc(table->max_rows * sizeof(size_t));

        // Allocate row arrays for existing rows
        for (int r = 0; r < table->num_rows; r++) {
            table->data[r] = (void **)malloc(table->num_columns * sizeof(void *));
        }

        for (int j = 0; j < table->num_rows; j++) {
            for (int k = 0; k < table->num_columns; k++) {
                switch (table->data_types[k]) {
                    case INTEGER:
                        table->data[j][k] = malloc(sizeof(int));
                        fread(table->data[j][k], sizeof(int), 1, file);
                        break;
                    case STRING: {
                        int string_length;
                        fread(&string_length, sizeof(int), 1, file);
                        table->data[j][k] = (char *)malloc((string_length + 1) * sizeof(char));
                        fread(table->data[j][k], sizeof(char), string_length, file);
                        ((char *)table->data[j][k])[string_length] = '\0';
                    } break;
                    case BLOB: {
                        size_t blob_size;
                        fread(&blob_size, sizeof(size_t), 1, file);
                        table->data[j][k] = malloc(blob_size);
                        fread(table->data[j][k], sizeof(unsigned char), blob_size, file);
                        table->data_sizes[j] = blob_size;
                    } break;
                }
            }
        }
    }

    fclose(file);
}


int tinysql() {
    database.tables = NULL;
    database.num_tables = 0;

    DataType person_data_types[] = {STRING, BLOB};
    create_table("Person", 2, person_data_types);

    char *john_name = "John";
    unsigned char john_blob[] = {0x05, 0x06, 0x88, 0x55, 0xAA};
    size_t john_blob_size = sizeof(john_blob);
    void *john_values[] = {john_name, john_blob};
    size_t john_sizes[] = {strlen(john_name), john_blob_size};
    insert_into_table("Person", john_values, john_sizes);

    char *alice_name = "Alice";
    unsigned char alice_blob[] = {0x07, 0x08, 0x33, 0x43, 0xFF};
    size_t alice_blob_size = sizeof(alice_blob);
    void *alice_values[] = {alice_name, alice_blob};
    size_t alice_sizes[] = {strlen(alice_name), alice_blob_size};
    insert_into_table("Person", alice_values, alice_sizes);

    select_from_table("Person");

    // Save the database to a file
   // save_database("database.dat");

    // Free the database to load it from the file later
    free_database();

    return 0;
}

int tinysql2() {
    // Load the database from the file
    load_database("database.dat");

    // Execute a SELECT statement to display the loaded table
    select_from_table("Person");

    // Free the database at the end of the program
    free_database();

    return 0;
}

// --- TinySQL FS helpers ----------------------------------------------------

static Table* find_table(const char* name) {
    for (int i = 0; i < database.num_tables; i++) {
        if (strcmp(database.tables[i].name, name) == 0) {
            return &(database.tables[i]);
        }
    }
    return NULL;
}

static int find_row_by_name(Table* t, const char* name) {
    for (int r = 0; r < t->num_rows; r++) {
        if (t->data_types[0] == STRING && t->data[r][0] != NULL) {
            if (strcmp((char*)t->data[r][0], name) == 0) return r;
        }
    }
    return -1;
}

void hbdb_ensure_fs_table(void) {
    Table* t = find_table("FS");
    if (t) return;
    DataType types[2] = { STRING, BLOB };
    create_table("FS", 2, types);
}

int hbdb_fs_get(const char* name, unsigned char** out_data, size_t* out_size) {
    Table* t = find_table("FS");
    if (!t) return 0;
    int row = find_row_by_name(t, name);
    if (row < 0) return 0;
    // column 1 is BLOB
    if (t->data[row][1] == NULL) { *out_data = NULL; *out_size = 0; return 1; }
    *out_size = t->data_sizes[row];
    unsigned char* buf = (unsigned char*)malloc(*out_size);
    memcpy(buf, t->data[row][1], *out_size);
    *out_data = buf;
    return 1;
}

int hbdb_fs_put(const char* name, const unsigned char* data, size_t size) {
    hbdb_ensure_fs_table();
    Table* t = find_table("FS");
    if (!t) return 0;
    int row = find_row_by_name(t, name);
    if (row < 0) {
        // insert new row
        void* values[2];
        values[0] = (void*)name;
        values[1] = (void*)data;
        size_t sizes[2];
        sizes[0] = strlen(name);
        sizes[1] = size;
        insert_into_table("FS", values, sizes);
        return 1;
    }
    // update existing row
    if (t->data[row][1]) free(t->data[row][1]);
    t->data[row][1] = malloc(size);
    memcpy(t->data[row][1], data, size);
    t->data_sizes[row] = size;
    return 1;
}

int hbdb_fs_remove(const char* name) {
    Table* t = find_table("FS");
    if (!t) return 0;
    int row = find_row_by_name(t, name);
    if (row < 0) return 0;
    // free columns in this row
    for (int k=0;k<t->num_columns;k++) {
        if (t->data[row][k]) free(t->data[row][k]);
    }
    free(t->data[row]);
    // shift rows up
    for (int r=row; r < t->num_rows-1; r++) {
        t->data[r] = t->data[r+1];
        t->data_sizes[r] = t->data_sizes[r+1];
    }
    t->num_rows--;
    return 1;
}

void hbdb_fs_list(void (*cb)(const char* name, size_t size)) {
    Table* t = find_table("FS");
    if (!t || !cb) return;
    for (int r = 0; r < t->num_rows; r++) {
        const char* nm = (const char*)t->data[r][0];
        size_t sz = t->data_sizes[r];
        if (nm) cb(nm, sz);
    }
}

// --- Serialization helpers (to memory buffer) ------------------------------
typedef struct { unsigned char* data; size_t len; size_t cap; } MemOut;
static void mo_init(MemOut* m){ m->data=NULL; m->len=0; m->cap=0; }
static void mo_write(MemOut* m, const void* src, size_t n){
    if (m->len + n > m->cap){ size_t nc = m->cap? m->cap:256; while (nc < m->len+n) nc*=2; unsigned char* nb=(unsigned char*)realloc(m->data, nc); if(!nb) return; m->data=nb; memset(m->data + m->cap, 0, nc - m->cap); m->cap=nc; }
    memcpy(m->data + m->len, src, n); m->len += n; }

int hbdb_serialize(unsigned char** out_buf, size_t* out_len) {
    if (!out_buf || !out_len) return 0;
    MemOut mo; mo_init(&mo);
    // write num_tables
    mo_write(&mo, &database.num_tables, sizeof(int));
    for (int i = 0; i < database.num_tables; i++) {
        Table *table = &(database.tables[i]);
        int name_length = strlen(table->name);
        mo_write(&mo, &name_length, sizeof(int));
        mo_write(&mo, table->name, name_length);
        mo_write(&mo, &table->num_columns, sizeof(int));
        mo_write(&mo, table->data_types, sizeof(DataType) * table->num_columns);
        mo_write(&mo, &table->num_rows, sizeof(int));
        for (int j = 0; j < table->num_rows; j++) {
            for (int k = 0; k < table->num_columns; k++) {
                switch (table->data_types[k]) {
                    case INTEGER: mo_write(&mo, table->data[j][k], sizeof(int)); break;
                    case STRING: {
                        int sl = strlen((char*)table->data[j][k]);
                        mo_write(&mo, &sl, sizeof(int));
                        mo_write(&mo, table->data[j][k], sl);
                    } break;
                    case BLOB: {
                        size_t bs = table->data_sizes[j];
                        mo_write(&mo, &bs, sizeof(size_t));
                        mo_write(&mo, table->data[j][k], bs);
                    } break;
                }
            }
        }
    }
    *out_buf = mo.data; *out_len = mo.len; return 1;
}

// try to parse with either 4-byte or 8-byte BLOB length fields (backward compatibility)
static int hbdb_deserialize_variant(const unsigned char* data, size_t len, int blob_len_size) {
    if (!data || len < sizeof(int)) return 0;
    const unsigned char* p = data; const unsigned char* end = data + len;
    free_database();
    if (p + 4 > end) return 0;
    memcpy(&database.num_tables, p, 4); p += 4;
    database.tables = (Table *)malloc(database.num_tables * sizeof(Table));
    if (!database.tables) return 0;
    for (int i = 0; i < database.num_tables; i++) {
        Table *table = &(database.tables[i]);
        int name_length = 0; if (p + 4 > end) return 0; memcpy(&name_length, p, 4); p += 4;
        table->name = (char *)malloc((name_length + 1)); if (!table->name || p + name_length > end) return 0; memcpy(table->name, p, name_length); p += name_length; table->name[name_length] = '\0';
        if (p + 4 > end) return 0; memcpy(&table->num_columns, p, 4); p += 4;
        table->data_types = (DataType *)malloc(table->num_columns * sizeof(DataType)); if (!table->data_types || p + table->num_columns*sizeof(DataType) > end) return 0; memcpy(table->data_types, p, table->num_columns*sizeof(DataType)); p += table->num_columns*sizeof(DataType);
        if (p + 4 > end) return 0; memcpy(&table->num_rows, p, 4); p += 4;
        table->max_rows = table->num_rows ? table->num_rows * 2 : 2;
        table->data = (void ***)malloc(table->max_rows * sizeof(void **)); if (!table->data) return 0;
        table->data_sizes = (size_t *)malloc(table->max_rows * sizeof(size_t)); if (!table->data_sizes) return 0;
        for (int r = 0; r < table->num_rows; r++) table->data[r] = (void **)malloc(table->num_columns * sizeof(void *));
        for (int j = 0; j < table->num_rows; j++) {
            for (int k = 0; k < table->num_columns; k++) {
                switch (table->data_types[k]) {
                    case INTEGER:
                        table->data[j][k] = malloc(sizeof(int)); if (!table->data[j][k] || p + 4 > end) return 0; memcpy(table->data[j][k], p, 4); p += 4; break;
                    case STRING: {
                        int sl = 0; if (p + 4 > end) return 0; memcpy(&sl, p, 4); p += 4;
                        table->data[j][k] = (char *)malloc(sl + 1); if (!table->data[j][k] || p + sl > end) return 0; memcpy(table->data[j][k], p, sl); p += sl; ((char*)table->data[j][k])[sl] = '\0';
                    } break;
                    case BLOB: {
                        size_t bs = 0; 
                        if (blob_len_size == 4) {
                            uint32_t t32 = 0; if (p + 4 > end) return 0; memcpy(&t32, p, 4); p += 4; bs = (size_t)t32;
                        } else {
                            uint64_t t64 = 0; if (p + 8 > end) return 0; memcpy(&t64, p, 8); p += 8; if (t64 > 0xFFFFFFFFu) return 0; bs = (size_t)(uint32_t)t64;
                        }
                        table->data[j][k] = malloc(bs); if (!table->data[j][k] || p + bs > end) return 0; memcpy(table->data[j][k], p, bs); p += bs; table->data_sizes[j] = bs;
                    } break;
                }
            }
        }
    }
    return 1;
}

int hbdb_deserialize(const unsigned char* data, size_t len) {
    // Try native 32-bit format first, then fallback to legacy 64-bit blob length
    if (hbdb_deserialize_variant(data, len, 4)) return 1;
    // If that failed, reset and try 8-byte blob length (legacy/host-64 tool variants)
    free_database();
    return hbdb_deserialize_variant(data, len, 8);
}

// ---- Image save/load and autosave -----------------------------------------
int hbdb_save_image(unsigned int lba_start) {
    unsigned char* buf=0; size_t len=0; if (!hbdb_serialize(&buf,&len)) return -1;
    if (len + 8 > g_hbdb_max_bytes){ free(buf); return -2; }
    unsigned int total = (unsigned int)(len + 8);
    unsigned int padded = (total + 511u) & ~511u;
    unsigned char* tmp=(unsigned char*)calloc(padded,1); if(!tmp){ free(buf); return -3; }
    unsigned int magic = 0x48424431; memcpy(tmp,&magic,4); memcpy(tmp+4,&len,4); memcpy(tmp+8,buf,len);
    // Prefer DMA write, fall back to fast PIO on error
    int dr = dma_write(lba_start, tmp, padded);
    if (dr != 0) {
        const unsigned int chunk_bytes = 128 * 512; // 64 KiB
        unsigned int written = 0;
        while (written < padded) {
            unsigned int n = padded - written;
            if (n > chunk_bytes) n = chunk_bytes;
            write_to_disk_fast(lba_start + (written/512), tmp + written, n);
            written += n;
        }
    }
    free(tmp); free(buf); return 0;
}

int hbdb_load_image(unsigned int lba_start) {
    // Step 1: read first 512 bytes for header (cheap PIO)
    unsigned char hdr[512]; memset(hdr,0,sizeof(hdr));
    read_from_disk_fast(lba_start, hdr, sizeof(hdr));
    unsigned int magic=0,len=0; memcpy(&magic,hdr,4); memcpy(&len,hdr+4,4);
    if (magic != 0x48424431) { return -3; }
    if (len + 8 > g_hbdb_max_bytes) { return -3; }
    unsigned int total = len + 8; unsigned int padded = (total + 511u) & ~511u;
    unsigned char* tmp = (unsigned char*)malloc(padded); if (!tmp) return -1;
    // Prefer DMA for the full payload; fall back to PIO on error
    int dr = dma_read(lba_start, tmp, padded);
    if (dr != 0) {
        const unsigned int chunk_bytes = 128 * 512;
        unsigned int rd = 0;
        while (rd < padded){
            unsigned int n = padded - rd; if (n > chunk_bytes) n = chunk_bytes;
            read_from_disk_fast(lba_start + (rd/512), tmp + rd, n);
            rd += n;
        }
    }
    int ok = hbdb_deserialize(tmp+8, len); free(tmp); return ok?0:-4;
}

void hbdb_set_autosave(int enable){ g_hbdb_autosave = enable ? 1 : 0; }
void hbdb_set_image_lba(unsigned int lba){ g_hbdb_image_lba = lba; }
void hbdb_autosave_maybe(void){ if (g_hbdb_autosave) { hbdb_save_image(g_hbdb_image_lba); } }

int hbdb_fs_rows(void){
    Table* t = find_table("FS");
    if (!t) return -1;
    return t->num_rows;
}

void hbdb_set_max_bytes(unsigned int max_bytes){ g_hbdb_max_bytes = max_bytes; }
unsigned int hbdb_get_max_bytes(void){ return g_hbdb_max_bytes; }
