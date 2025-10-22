
typedef void (*FunctionPointer)();

static char kernel_console_key_buffer[1024];

void kernel_request_vt_switch(int index);
void kernel_process_pending_vt(void);

#define ET(x) printf("Error Trace %s\n", #x)

#define KERNEL_PROMPT_UNKNOWN_COMMAND "Unknown command:"

// Makro, um eine Funktion nach Namen aufzurufen (ohne Parameter)
#define CALL_FUNCTION_ALIAS(funcName, alias) \
    else if (strcmp(input, #funcName) == 0) { \
        alias(); \
    }

// Makro, um eine Funktion nach Namen aufzurufen (ohne Parameter)
#define CALL_FUNCTION(funcName) \
    else if (strcmp(input, #funcName) == 0) { \
        funcName(); \
    }

// exact/prefix match helper: command must be followed by NUL or space
#define CMD_MATCH(name) (strncmp(input, name, strlen(name))==0 && \
                        (input[strlen(name)]=='\0' || input[strlen(name)]==' '))

// Makro, um eine Funktion nach Namen aufzurufen (mit einem String-Parameter)
#define CALL_FUNCTION_WITH_STR(funcName) \
    else if (CMD_MATCH(#funcName)) { \
        char* __p = input + strlen(#funcName); \
        if (*__p == ' ') __p++; \
        char* str1 = strtok(__p, " "); \
        if (str1 != NULL) { \
            funcName(str1); \
        } else { printf("Invalid parameters for %s\n", #funcName); } \
    }

// Optional string parameter: calls with empty string if none provided
#define CALL_FUNCTION_WITH_OPT_STR(funcName) \
    else if (CMD_MATCH(#funcName)) { \
        char* __p = input + strlen(#funcName); \
        const char* arg = ""; \
        if (*__p == ' ') { __p++; char* t = strtok(__p, " "); if (t) arg = t; } \
        funcName(arg); \
    }

// Makro, um eine Funktion nach Namen aufzurufen (mit einem Parameter)
#define CALL_FUNCTION_WITH_ARG(funcName) \
    else if (CMD_MATCH(#funcName)) { \
        char* __p = input + strlen(#funcName); \
        if (*__p == ' ') __p++; \
        char* str1 = strtok(__p, " "); \
        if (str1 != NULL) { \
            uint32_t param1 = (uint32_t)strtoul(str1, NULL, 0); \
            funcName((void*)param1); \
        } else { printf("Invalid parameters for %s\n", #funcName); } \
    }

// Makro, um eine Funktion nach Namen aufzurufen (mit drei Parametern)
#define CALL_FUNCTION_WITH_2ARGS(funcName) \
    else if (CMD_MATCH(#funcName)) { \
        char* __p = input + strlen(#funcName); \
        if (*__p == ' ') __p++; \
        char* str1 = strtok(__p, " "); \
        char* str2 = strtok(NULL, " "); \
        if (str1 != NULL && str2 != NULL) { \
            uint32_t param1 = (uint32_t)strtoul(str1, NULL, 0); \
            uint32_t param2 = (uint32_t)strtoul(str2, NULL, 0); \
            funcName(param1, param2); \
        } else { printf("Invalid parameters for %s\n", #funcName); } \
    }


// Makro, um eine Funktion nach Namen aufzurufen (mit drei Parametern)
#define CALL_FUNCTION_WITH_2ARGS_AND_STR(funcName) \
    else if (CMD_MATCH(#funcName)) { \
        char* __p = input + strlen(#funcName); \
        if (*__p == ' ') __p++; \
        char* str1 = strtok(__p, " "); \
        char* str2 = strtok(NULL, " "); \
        char* str3 = strtok(NULL, " "); \
        if (str1 != NULL && str2 != NULL && str3 != NULL) { \
            uint32_t param1 = (uint32_t)strtoul(str1, NULL, 0); \
            uint32_t param2 = (uint32_t)strtoul(str2, NULL, 0); \
            funcName(param1, param2, str3); \
        } else { printf("Invalid parameters for %s\n", #funcName); } \
    }



// Makro, um eine Funktion nach Namen aufzurufen (mit drei Parametern)
#define CALL_FUNCTION_WITH_3ARGS(funcName) \
    else if (CMD_MATCH(#funcName)) { \
        char* __p = input + strlen(#funcName); \
        if (*__p == ' ') __p++; \
        char* str1 = strtok(__p, " "); \
        char* str2 = strtok(NULL, " "); \
        char* str3 = strtok(NULL, " "); \
        if (str1 != NULL && str2 != NULL && str3 != NULL) { \
            uint32_t param1 = (uint32_t)strtoul(str1, NULL, 0); \
            uint32_t param2 = (uint32_t)strtoul(str2, NULL, 0); \
            uint32_t param3 = (uint32_t)strtoul(str3, NULL, 0); \
            funcName(param1, param2, param3); \
        } else { printf("Invalid parameters for %s\n", #funcName); } \
    }

void kernel_main();
void kernel_init();
void kernel_exit();
void kernel_panic(char *message);

void execute_command(char *input);
