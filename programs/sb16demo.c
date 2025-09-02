// sb16demo.c – SB16 minimal DMA test mit fixem Lowmem-Buffer
#include <stdint.h>

#define DSP_RESET   0x226
#define DSP_READ    0x22A
#define DSP_WRITE   0x22C
#define DSP_STATUS  0x22E

extern void port_byte_out(uint16_t, uint8_t);
extern uint8_t port_byte_in(uint16_t);

#define dbg_putc(c) port_byte_out(0xE9, (c))
static void dbg_hex(uint32_t v) {
    const char *hex = "0123456789ABCDEF";
    for (int i=28; i>=0; i-=4)
        dbg_putc(hex[(v >> i) & 0xF]);
}
static void dbg_puts(const char *s) {
    while (*s) dbg_putc(*s++);
}

#define DMA_ADDR 0x20000
#define DMA_SIZE 1024

// ---- DMA1 setup ----
static void dma_setup_1(uint32_t addr, uint32_t size) {
    uint16_t offset = addr & 0xFFFF;
    uint8_t  page   = (addr >> 16) & 0xFF;
    uint16_t count  = size - 1;

    dbg_puts("dma_setup @");
    dbg_hex(addr);
    dbg_putc('\n');

    port_byte_out(0x0A, 0x05);        // mask chan1
    port_byte_out(0x0C, 0x00);        // clear flip-flop
    port_byte_out(0x02, offset & 0xFF);
    port_byte_out(0x02, (offset >> 8) & 0xFF);
    port_byte_out(0x83, page);        // page register
    port_byte_out(0x0C, 0x00);        // clear flip-flop again
    port_byte_out(0x03, count & 0xFF);
    port_byte_out(0x03, (count >> 8) & 0xFF);
    port_byte_out(0x0B, 0x49);        // read, auto-init, inc, single
    port_byte_out(0x0A, 0x01);        // unmask
}

// ---- DSP helpers ----
static void dsp_reset(void) {
    dbg_puts("dsp_reset\n");
    port_byte_out(DSP_RESET, 1);
    for (volatile int i=0; i<1000; i++);
    port_byte_out(DSP_RESET, 0);
    while (!(port_byte_in(DSP_STATUS) & 0x80));
    (void)port_byte_in(DSP_READ); // expect 0xAA
    dbg_puts("dsp_ok\n");
}
static void dsp_write(uint8_t val) {
    while (port_byte_in(DSP_WRITE) & 0x80);
    port_byte_out(DSP_WRITE, val);
}

// ---- main demo ----
int sb16demo(void) {
    dbg_puts("sb16demo start\n");

    // Buffer bei 0x20000 befüllen
    uint8_t *buf = (uint8_t*)DMA_ADDR;
    for (int i=0; i<DMA_SIZE; i++)
        buf[i] = i & 0xFF;  // Sägezahn

    dsp_reset();
    dsp_write(0xD1); // speaker on

    int rate = 8000;
    dsp_write(0x41);
    dsp_write((rate >> 8) & 0xFF);
    dsp_write(rate & 0xFF);

    dma_setup_1(DMA_ADDR, DMA_SIZE);

    dbg_puts("start playback\n");
    dsp_write(0x1C);  // 8-bit auto-init DMA
    dsp_write((DMA_SIZE-1) & 0xFF);
    dsp_write((DMA_SIZE-1) >> 8);

    for (volatile int i=0; i<100000000; i++);
    dsp_write(0xD3); // speaker off

    dbg_puts("sb16demo done\n");
    return 0;
}
