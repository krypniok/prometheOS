#include <stdint.h>
#include <stddef.h>

#include "net.h"
#include "ports.h"
#include "pci.h"
#include "debug.h"
#include "display.h"
#include "../net/net.h"
#include "../kernel/time.h"

extern void memcpy(void *dest, const void *src, size_t count);
extern void memset(void *dest, int value, size_t count);

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139
#define RTL8139_RX_CAPACITY 8192
#define RTL8139_RX_BUFFER_SIZE (RTL8139_RX_CAPACITY + 16 + 1500)
#define RTL8139_TX_BUFFER_SIZE 1536
#define RTL8139_TX_SLOTS 4

static uint16_t g_io_base;
static uint8_t g_irq_line;
static uint8_t g_mac[6];
static uint8_t g_initialized;
static uint8_t g_tx_slot;

static uint8_t g_rx_buffer[RTL8139_RX_BUFFER_SIZE] __attribute__((aligned(4)));
static uint8_t g_tx_buffer[RTL8139_TX_SLOTS][RTL8139_TX_BUFFER_SIZE] __attribute__((aligned(4)));
static uint32_t g_rx_offset;

static uint8_t g_pci_bus;
static uint8_t g_pci_slot;
static uint8_t g_pci_func;

static void rtl8139_process_rx(void) {
    if (!g_initialized) {
        return;
    }

    while (1) {
        uint16_t status = *(uint16_t*)(g_rx_buffer + g_rx_offset);
        if (!(status & 0x01)) {
            break;
        }

        uint16_t length = *(uint16_t*)(g_rx_buffer + g_rx_offset + 2);
        if (length < 4 || length > RTL8139_RX_BUFFER_SIZE) {
            // Invalid length, drop and reset
            debug_puts("[net] RX invalid len\n");
            g_rx_offset = 0;
            port_word_out(g_io_base + 0x38, 0);
            break;
        }

        const uint8_t *frame = g_rx_buffer + g_rx_offset + 4;
        uint16_t frame_len = (length >= 4) ? (uint16_t)(length - 4) : 0;
        if (frame_len > 0) {
            debug_puts("[net] RX frame\n");
            net_stack_handle_frame(frame, frame_len);
        }

        uint32_t offset = g_rx_offset + length + 4;
        offset = (offset + 3u) & ~3u; // align next packet pointer
        offset &= (RTL8139_RX_CAPACITY - 1);
        g_rx_offset = offset;

        uint16_t capr = (uint16_t)((g_rx_offset - 16) & (RTL8139_RX_CAPACITY - 1));
        port_word_out(g_io_base + 0x38, capr);
    }
}

static int rtl8139_find(uint8_t *bus, uint8_t *slot, uint8_t *func) {
    for (uint16_t b = 0; b < 256; ++b) {
        for (uint8_t s = 0; s < 32; ++s) {
            for (uint8_t f = 0; f < 8; ++f) {
                uint32_t id = pci_config_read_dword((uint8_t)b, s, f, 0x00);
                if ((id & 0xFFFFu) == 0xFFFFu) {
                    continue;
                }
                uint16_t vendor = (uint16_t)(id & 0xFFFFu);
                uint16_t device = (uint16_t)((id >> 16) & 0xFFFFu);
                if (vendor == RTL8139_VENDOR_ID && device == RTL8139_DEVICE_ID) {
                    if (bus) { *bus = (uint8_t)b; }
                    if (slot) { *slot = s; }
                    if (func) { *func = f; }
                    return 0;
                }
            }
        }
    }
    return -1;
}

static int rtl8139_reset(uint16_t io_base) {
    port_byte_out(io_base + 0x52, 0x00); // Config1: power on
    port_byte_out(io_base + 0x37, 0x10); // Send reset command
    for (int i = 0; i < 100000; ++i) {
        if (!(port_byte_in(io_base + 0x37) & 0x10)) {
            return 0;
        }
        io_wait();
    }
    return -1;
}

int net_init(void) {
    if (g_initialized) {
        return 0;
    }

    if (rtl8139_find(&g_pci_bus, &g_pci_slot, &g_pci_func) != 0) {
        printf("[net] Keine RTL8139-Netzwerkkarte gefunden.\n");
        return -1;
    }

    uint32_t bar0 = pci_config_read_dword(g_pci_bus, g_pci_slot, g_pci_func, 0x10);
    if (!(bar0 & 0x01u)) {
        printf("[net] Erwartete IO-Basisadresse, bekam MMIO (%08x).\n", bar0);
        return -1;
    }
    g_io_base = (uint16_t)(bar0 & ~0x03u);

    pci_enable_bus_master(g_pci_bus, g_pci_slot, g_pci_func);
    uint16_t cmd = pci_config_read_word(g_pci_bus, g_pci_slot, g_pci_func, 0x04);
    if (!(cmd & 0x0001u)) {
        cmd |= 0x0001u; // I/O space enable
        pci_config_write_word(g_pci_bus, g_pci_slot, g_pci_func, 0x04, cmd);
    }

    if (rtl8139_reset(g_io_base) != 0) {
        printf("[net] RTL8139 Reset schlug fehl.\n");
        return -1;
    }

    for (int i = 0; i < 6; ++i) {
        g_mac[i] = port_byte_in(g_io_base + i);
    }

    uint32_t irq_reg = pci_config_read_dword(g_pci_bus, g_pci_slot, g_pci_func, 0x3C);
    g_irq_line = (uint8_t)(irq_reg & 0xFFu);

    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    port_long_out(g_io_base + 0x30, (uint32_t)g_rx_buffer);
    g_rx_offset = 0;
    port_word_out(g_io_base + 0x38, (uint16_t)(RTL8139_RX_CAPACITY - 16));

    port_word_out(g_io_base + 0x3C, 0x0005); // RxOK, TxOK interrupts
    port_word_out(g_io_base + 0x3E, 0xFFFF); // clear pending

    port_long_out(g_io_base + 0x44, 0x0000075F); // Receive config: accept broadcast/multicast, wrap
    port_long_out(g_io_base + 0x40, 0x03000700); // Transmit config: DMA burst + threshold

    g_tx_slot = 0;
    for (int i = 0; i < RTL8139_TX_SLOTS; ++i) {
        memset(g_tx_buffer[i], 0, RTL8139_TX_BUFFER_SIZE);
        port_long_out(g_io_base + 0x20 + (i * 4), (uint32_t)g_tx_buffer[i]);
    }

    port_byte_out(g_io_base + 0x37, 0x0C); // Enable Tx + Rx

    g_initialized = 1;

    net_stack_init(g_mac);

    printf("[net] RTL8139 @0x%X IRQ %d MAC %02X:%02X:%02X:%02X:%02X:%02X initialisiert.\n",
           g_io_base,
           g_irq_line,
           g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5]);

    return 0;
}

int net_send(const void *data, uint32_t length) {
    if (!g_initialized) {
        debug_puts("[net] send fail not init\n");
        return -1;
    }
    if (!data || length == 0 || length > RTL8139_TX_BUFFER_SIZE) {
        debug_puts("[net] send invalid len\n");
        return -1;
    }

    debug_puts("[net] send begin\n");
    debug_puthex((uint32_t)length);
    debug_puts(" len\n");
    uint32_t copy_len = length;
    if (copy_len < 60u) {
        copy_len = 60u; // Ethernet minimum frame length
    }

    memcpy(g_tx_buffer[g_tx_slot], data, length);
    debug_puts("[net] memcpy done\n");
    if (copy_len > length) {
        memset(g_tx_buffer[g_tx_slot] + length, 0, copy_len - length);
    }
    debug_puts("[net] memset done\n");

    uint16_t addr_reg = g_io_base + 0x20 + (g_tx_slot * 4);
    uint16_t stat_reg = g_io_base + 0x10 + (g_tx_slot * 4);

    debug_puts("[net] write addr reg\n");
    port_long_out(addr_reg, (uint32_t)g_tx_buffer[g_tx_slot]);
    debug_puts("[net] addr written\n");
    debug_puts("[net] write status\n");
    uint32_t cmd = copy_len & 0x1FFFu; // length only
    cmd |= (1u << 13); // OWN bit to hand to NIC
    port_long_out(stat_reg, cmd);
    uint32_t tsd = port_long_in(stat_reg);
    debug_puts("[net] tsd=\n");
    debug_puthex(tsd);
    debug_puts("\n");
    debug_puts("[net] send posted\n");

    g_tx_slot = (uint8_t)((g_tx_slot + 1) % RTL8139_TX_SLOTS);
    return 0;
}

void net_dump_regs(void) {
    if (!g_initialized) {
        printf("[netdump] Karte nicht initialisiert.\n");
        return;
    }

    uint32_t rbstart = g_rx_buffer ? (uint32_t)g_rx_buffer : 0;
    uint16_t capr = port_word_in(g_io_base + 0x38);
    uint16_t cbr  = port_word_in(g_io_base + 0x3A);
    printf("[netdump] RBSTART=%08X CAPR=%04X CBR=%04X\n", rbstart, capr, cbr);

    for (int slot = 0; slot < RTL8139_TX_SLOTS; ++slot) {
        uint16_t addr_reg = g_io_base + 0x20 + slot * 4;
        uint16_t stat_reg = g_io_base + 0x10 + slot * 4;
        uint32_t tsad = port_long_in(addr_reg);
        uint32_t tsd  = port_long_in(stat_reg);
        printf("[netdump] TX%d TSAD=%08X TSD=%08X\n", slot, tsad, tsd);
    }

    uint16_t isr = port_word_in(g_io_base + 0x3E);
    printf("[netdump] ISR=%04X\n", isr);
}

const uint8_t *net_get_mac(void) {
    return g_initialized ? g_mac : NULL;
}

void net_info(void) {
    if (!g_initialized) {
        printf("[net] Keine Karte initialisiert.\n");
        return;
    }
    printf("[net] RTL8139 @0x%X IRQ %d MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
           g_io_base,
           g_irq_line,
           g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5]);
}

void net_poll(void) {
    if (!g_initialized) {
        return;
    }

    rtl8139_process_rx();

    uint16_t isr = port_word_in(g_io_base + 0x3E);
    if (isr) {
        port_word_out(g_io_base + 0x3E, isr);
    }
}

void net_tick(void) {
    if (!g_initialized) {
        return;
    }
    net_stack_tick();
}
