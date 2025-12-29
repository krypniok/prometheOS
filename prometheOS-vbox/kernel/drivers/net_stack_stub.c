#include "../../include/drivers/net.h"
#include "../../include/drivers/display.h"

void net_stack_init(const uint8_t* mac) {
    printf("[NET] Stack stub initialized (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void net_stack_handle_frame(const uint8_t* data, uint16_t len) {
    (void)data;
    // Nur Debug-Ausgabe, dass ein Paket ankam
    // printf("[NET] Frame received (%d bytes)\n", len);
}

void net_stack_tick(void) {
    // Periodic tick (z.B. für DHCP Retransmission oder TCP Timer)
}

int net_ping(uint32_t dest_ip, int timeout_ms) {
    printf("[NET] Ping stub to %08X (timeout %d)\n", dest_ip, timeout_ms);
    return -1;
}