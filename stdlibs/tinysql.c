#include <stddef.h>

#include "file.h"
#include "string.h"
#include "memory.h"


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

void create_table(char *table_name, int num_columns, DataType data_types[]) {
    if (database.num_tables == 0) {
        database.tables = (Table *)malloc(sizeof(Table));
    } else {
        database.tables = (Table *)realloc(database.tables, (database.num_tables + 1) * sizeof(Table));
    }

    Table *new_table = &(database.tables[database.num_tables]);
    new_table->name = strdup(table_name);
    new_table->data = (void ***)malloc(2 * sizeof(void **)); // Start with space for 2 rows
    new_table->data_sizes = (size_t *)malloc(num_columns * sizeof(size_t)); // Reserve memory for BLOB sizes
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
    free(database.tables);
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
