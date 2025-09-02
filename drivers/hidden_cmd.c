#include <stddef.h>
#include <stdbool.h>

#include "hidden_cmd.h"
#include "display.h"

#include "../stdlibs/string.h"


#define MAX_BUFFER_LENGTH 10


bool is_system_pattern(const char *buffer) {
    return strncmp(buffer, "system", strlen("system")) == 0 &&
           isdigit(buffer[strlen("system")]) &&
           isdigit(buffer[strlen("system") + 1]) &&
           isdigit(buffer[strlen("system") + 2]) &&
           isdigit(buffer[strlen("system") + 3]);
}

void print_buffer(unsigned char* buffer) {
    int cursor = get_cursor();
    set_cursor(4000 - 160);
    print_string(buffer);
    set_cursor(cursor);
}

char buffer[MAX_BUFFER_LENGTH + 1];
int buffer_length = 0;
//int input;

void process_value(const char *value) {
    print_buffer(buffer);    
    //printf("Gefundener Wert: %s\n", value);
    // Hier kannst du deine gewünschte Aktion ausführen, z.B. system aufrufen
}

bool handle_invisible_keypress(unsigned char input) {
        if (input != 0) {
            char lowercase_input = tolower(input);
            if (isalnum(lowercase_input) && buffer_length < MAX_BUFFER_LENGTH) {
                buffer[buffer_length++] = lowercase_input;
                buffer[buffer_length] = '\0';
                
                if (is_system_pattern(buffer)) {
                    if (buffer_length == strlen("system") + 4) {
                        char value[5];
                        strncpy(value, buffer + strlen("system"), 4);
                        value[4] = '\0';
                        //buffer[0] = '\0';
                        process_value(value);
                        buffer_length = 0;
                    }
                }
            } else {
                buffer_length = 0;
            }
        }
}
