#include "../../include/drivers/display.h"
#include "../../include/drivers/net.h"
#include "../../include/lib/string.h"

void netcat_cmd(const char* args) {
    if (!args || !*args) {
        printf("Usage: netcat <payload>\n");
        return;
    }
    
    printf("[NETCAT] Sending: %s\n", args);
    net_send(args, strlen(args));
}