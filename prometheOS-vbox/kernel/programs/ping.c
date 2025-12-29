#include "../../include/drivers/display.h"
#include "../../include/drivers/net.h"
#include "../../include/lib/string.h"
void net_ping_cmd(const char* args) {
    printf("[PING] Sending ping to %s (stub)...\n", args ? args : "broadcast");
    // TODO: Implement ICMP
    net_send("PING", 4);
}