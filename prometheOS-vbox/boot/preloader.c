__asm__(".code16gcc");

// Globales Debug-Flag
// 0: Keine, 1: COM1, 2: BIOS, 3: Beide
static unsigned char debug_mode = 3;

#define COM1 0x3F8

// Port I/O Helper
static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm__ volatile ("inb %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %w1" : : "a"(val), "Nd"(port));
}

// COM1 Ausgabe
static inline void com1_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20));
    outb(COM1, (unsigned char)c);
}

// BIOS Ausgabe (INT 0x10) - Nur im 16-Bit Modus nutzen!
static inline void bios_putc(char c) {
    __asm__ volatile (
        "movb $0x0E, %%ah\n\t"
        "int $0x10\n\t"
        : : "a"(c) : "bx"
    );
}

// Zentrale Ausgabe-Funktion
static inline void putc(char c) {
    if (debug_mode & 1) com1_putc(c);
    if (debug_mode & 2) bios_putc(c);
}

static void puts(const char *s) {
    while (*s) {
        putc(*s);
        s++;
    }
}

// Hex-Ausgabe Helper
static void print_hex8(unsigned char val) {
    const char *hex = "0123456789ABCDEF";
    putc(hex[(val >> 4) & 0xF]);
    putc(hex[val & 0xF]);
}

static void print_hex16(unsigned short val) {
    print_hex8((val >> 8) & 0xFF);
    print_hex8(val & 0xFF);
}

static void print_hex32(unsigned int val) {
    print_hex16((val >> 16) & 0xFFFF);
    print_hex16(val & 0xFFFF);
}

// VBE Info Block Struktur (VESA BIOS Extension)
struct vbe_info_struct {
    char signature[4];          // "VESA"
    unsigned short version;     // VBE Version
    unsigned int oem_ptr;       // Pointer to OEM String
    unsigned int capabilities;  // Capabilities of graphics controller
    unsigned int video_mode_ptr;// Pointer to VideoModeList
    unsigned short total_memory;// Number of 64kb memory blocks
    unsigned short oem_sw_rev;  // OEM Software Revision
    unsigned int oem_vendor_name_ptr;
    unsigned int oem_product_name_ptr;
    unsigned int oem_product_rev_ptr;
    unsigned char reserved[222];
    unsigned char oem_data[256];
} __attribute__((packed));

// VBE Mode Info (wird von INT 10h AX=4F01 geliefert)
struct vbe_mode_info_struct {
    unsigned short attributes;
    unsigned char window_a, window_b;
    unsigned short granularity, window_size, segment_a, segment_b;
    unsigned int win_func_ptr;
    unsigned short pitch; // Bytes pro Zeile
    unsigned short width, height;
    unsigned char w_char, y_char, planes, bpp, banks;
    unsigned char memory_model, bank_size, image_pages;
    unsigned char reserved0;
    unsigned char red_mask, red_pos, green_mask, green_pos, blue_mask, blue_pos, res_mask, res_pos, direct_color_attributes;
    unsigned int framebuffer; // Physikalische Adresse des LFB
    unsigned int off_screen_mem_off;
    unsigned short off_screen_mem_size;
    unsigned char reserved1[206];
} __attribute__((packed));

// C-Logik vor dem Switch (läuft im Real Mode)
void real_mode_logic() {
    __asm__ volatile ("sti"); // Interrupts an für BIOS-Calls
    puts("[PRELOADER] Started at 0x10000\r\n");
    
    // --- Memory Detection ---
    // INT 0x88 verursacht Hänger. Wir setzen fest 64MB (passend zum Makefile).
    unsigned int total_mem_kb = 65536;
    unsigned short mem_lo = total_mem_kb & 0xFFFF;
    unsigned short mem_hi = (total_mem_kb >> 16) & 0xFFFF;

    // Schreibe RAM-Größe nach 0x5100 (via ES=0)
    __asm__ volatile (
        "pushw %%es\n\t"
        "xor %%bx, %%bx\n\t"
        "mov %%bx, %%es\n\t"
        "mov %0, %%es:0x5100\n\t"
        "mov %1, %%es:0x5102\n\t"
        "popw %%es\n\t"
        : : "a"(mem_lo), "c"(mem_hi) : "bx", "memory"
    );

    // --- VBE Detection ---
    // Wir nutzen Buffer bei physikalisch 0x5000.
    // Da DS=0x1000 ist, können wir per C-Pointer nicht direkt auf 0x5000 zugreifen (wäre 0x15000).
    // Wir nutzen Inline-Assembly mit ES=0.

    unsigned short ax_ret;
    __asm__ volatile (
        "pushw %%es\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%es\n\t"     // ES = 0
        "mov $0x5000, %%di\n\t"  // ES:DI = 0x0000:0x5000 (Physikalisch 0x5000)
        
        "movl $0x32454256, %%es:(%%di)\n\t" // Signatur "VBE2" schreiben ('V' 'B' 'E' '2')
        
        "mov $0x4F00, %%ax\n\t"  // VBE Get Controller Info
        "int $0x10\n\t"
        "popw %%es\n\t"
        "mov %%ax, %0\n\t"
        : "=r"(ax_ret) : : "ax", "di", "memory"
    );

    if (ax_ret == 0x004F) {
        puts("[VBE] Detected (written to 0x5000)\r\n");
    } else {
        puts("[VBE] Error or not supported.\r\n");
    }

    // --- Save BIOS Font & Text RAM ---
    puts("Saving BIOS Font & Text RAM...\r\n");
    
    // 1. BIOS Font (8x16) holen (INT 10h AX=1130h BH=06h -> ES:BP)
    // Wir kopieren 4KB (256 Zeichen * 16 Bytes) nach 0x6000
    __asm__ volatile (
        "pushw %%es\n\t"
        "pushw %%bp\n\t"
        "mov $0x1130, %%ax\n\t"
        "mov $0x06, %%bh\n\t"
        "int $0x10\n\t"         // Returns ES:BP
        "pushw %%ds\n\t"
        "pushw %%es\n\t"
        "popw %%ds\n\t"          // DS = Font Segment
        "mov %%bp, %%si\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%es\n\t"
        "mov $0x6000, %%di\n\t" // Ziel: 0000:6000
        "mov $4096, %%cx\n\t"   // 256 * 16 bytes
        "rep movsb\n\t"
        "popw %%ds\n\t"
        "popw %%bp\n\t"
        "popw %%es\n\t"
        : : : "ax", "bx", "cx", "dx", "si", "di", "memory"
    );

    // 2. Text RAM (0xB8000) retten nach 0x7000
    // Damit wir den Start-Text im Kernel wiederherstellen können
    __asm__ volatile (
        "pushw %%ds\n\t"
        "pushw %%es\n\t"
        "mov $0xB800, %%ax\n\t"
        "mov %%ax, %%ds\n\t"    // DS = 0xB800
        "xor %%si, %%si\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%es\n\t"
        "mov $0x7000, %%di\n\t" // Ziel: 0000:7000
        "mov $4000, %%cx\n\t"   // 80 * 25 * 2 bytes
        "rep movsb\n\t"
        "popw %%es\n\t"
        "popw %%ds\n\t"
        : : : "ax", "cx", "si", "di", "memory"
    );

    // --- VBE Mode Switch (800x600x32) ---
    // Wir suchen einen passenden Modus.
    // Info: Wir nutzen hier vereinfacht einen festen Modus-Scan oder Standard-Modi.
    // VirtualBox unterstützt oft VBE-Modi linear.
    
    unsigned short mode = 0;
    unsigned int lfb_addr = 0;
    unsigned short best_width = 0, best_height = 0, best_pitch = 0;
    unsigned char best_bpp = 0;
    
    // Buffer für Mode-Info bei 0x5200
    struct vbe_mode_info_struct *mode_info = (struct vbe_mode_info_struct *)0x5200;

    // Get video_mode_ptr from VBE Info (0x5000 + 14 = 0x500E)
    unsigned short mode_list_off, mode_list_seg;
    __asm__ volatile (
        "pushw %%es\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%es:0x500E, %0\n\t"
        "mov %%es:0x5010, %1\n\t"
        "popw %%es\n\t"
        : "=r"(mode_list_off), "=r"(mode_list_seg) : : "ax", "memory"
    );

    // Iterate mode list
    short modes_checked = 0;
    while (1) {
        unsigned short m;
        // Read mode from far pointer (mode_list_seg:mode_list_off)
        __asm__ volatile (
            "pushw %%es\n\t"
            "mov %1, %%es\n\t"
            "mov %2, %%di\n\t"
            "mov %%es:(%%di), %0\n\t"
            "popw %%es\n\t"
            : "=r"(m) : "r"(mode_list_seg), "r"(mode_list_off) : "di", "memory"
        );

        if (m == 0xFFFF) break; // End of list
        mode_list_off += 2; // Next mode

        unsigned short ax_ret;
        __asm__ volatile (
            "pushw %%es\n\t"
            "xor %%ax, %%ax\n\t"
            "mov %%ax, %%es\n\t"
            "mov $0x5200, %%di\n\t"
            "mov $0x4F01, %%ax\n\t" // Get Mode Info
            "mov %1, %%cx\n\t"      // Mode Number
            "int $0x10\n\t"
            "popw %%es\n\t"
            "mov %%ax, %0\n\t"
            : "=r"(ax_ret) : "r"(m) : "ax", "cx", "di", "memory"
        );

        if (ax_ret == 0x004F) {
            modes_checked++;
            // Prüfen: Support (Bit 0), Graphics (Bit 4), LFB (Bit 7)
            // Wir lesen die Werte via ES=0 Segment Trick, da mode_info Pointer im DS=0x1000 Kontext falsch wäre
            unsigned short width, height, attr, pitch;
            unsigned char bpp;
            unsigned int fb;
            unsigned short fb_lo, fb_hi; // Helper für 32-Bit Split
            
            __asm__ volatile (
                "pushw %%es\n\t"
                "xor %%bx, %%bx\n\t"
                "mov %%bx, %%es\n\t"
                "mov %%es:0x5200, %0\n\t"
                "popw %%es\n\t"
                : "=r"(attr) : : "bx"
            );
            __asm__ volatile (
                "pushw %%es\n\t"
                "xor %%bx, %%bx\n\t"
                "mov %%bx, %%es\n\t"
                "mov %%es:0x5212, %0\n\t"
                "popw %%es\n\t"
                : "=r"(width) : : "bx"
            );
            __asm__ volatile (
                "pushw %%es\n\t"
                "xor %%bx, %%bx\n\t"
                "mov %%bx, %%es\n\t"
                "mov %%es:0x5214, %0\n\t"
                "popw %%es\n\t"
                : "=r"(height) : : "bx"
            );
            __asm__ volatile (
                "pushw %%es\n\t"
                "xor %%bx, %%bx\n\t"
                "mov %%bx, %%es\n\t"
                "mov %%es:0x5210, %0\n\t"
                "popw %%es\n\t"
                : "=r"(pitch) : : "bx"
            );
            __asm__ volatile (
                "pushw %%es\n\t"
                "xor %%bx, %%bx\n\t"
                "mov %%bx, %%es\n\t"
                "movb %%es:0x5219, %0\n\t"
                "popw %%es\n\t"
                : "=q"(bpp) : : "bx"
            );
            // Lese FB Adresse als zwei 16-Bit Werte, um 32-Bit Register Probleme zu vermeiden
             __asm__ volatile (
                "pushw %%es\n\t"
                "xor %%bx, %%bx\n\t"
                "mov %%bx, %%es\n\t"
                "mov %%es:0x5228, %0\n\t"
                "popw %%es\n\t"
                : "=r"(fb_lo) : : "bx"
            );
             __asm__ volatile (
                "pushw %%es\n\t"
                "xor %%bx, %%bx\n\t"
                "mov %%bx, %%es\n\t"
                "mov %%es:0x522A, %0\n\t"
                "popw %%es\n\t"
                : "=r"(fb_hi) : : "bx"
            );
            fb = ((unsigned int)fb_hi << 16) | fb_lo;

            // DEBUG: Zeige die ersten 5 Modi an, um zu sehen, was verfügbar ist
            // if (modes_checked <= 5) {
            //    puts("Chk: "); print_hex16(width); puts("x"); print_hex16(height); 
            //    puts("x"); print_hex8(bpp); puts(" Attr:"); print_hex16(attr); puts("\r\n");
            // }

            // Check: 800x600, 32bpp, LFB support (bit 7 of attr), Graphics (bit 4)
            // Relaxed: Allow 24bpp or 32bpp to find a mode in VirtualBox
            if (width == 800 && height == 600 && (bpp == 32 || bpp == 24) && (attr & 0x90) == 0x90) {
                mode = m;
                lfb_addr = fb;
                best_width = width;
                best_height = height;
                best_pitch = pitch;
                best_bpp = bpp;
                break;
            }
        }
    }
    
    if (modes_checked == 0) puts("No modes checked (List empty?)\r\n");

    if (mode != 0 && lfb_addr != 0) {
        puts("Setting VBE Mode 800x600x32...\r\n");
        unsigned short ax_ret;
        __asm__ volatile (
            "mov $0x4F02, %%ax\n\t"
            "mov %1, %%bx\n\t"
            "or $0x4000, %%bx\n\t" // Use LFB (Linear Framebuffer)
            "int $0x10\n\t"
            "mov %%ax, %0\n\t"
            : "=r"(ax_ret) : "r"(mode) : "ax", "bx"
        );
        
        // Speichere Video-Infos strukturiert an 0x5200 für den Kernel
        // 0x5200: LFB (4)
        // 0x5204: Width (2), 0x5206: Height (2), 0x5208: Pitch (2), 0x520A: BPP (1)
        unsigned short lfb_lo = lfb_addr & 0xFFFF;
        unsigned short lfb_hi = (lfb_addr >> 16) & 0xFFFF;
        
        __asm__ volatile (
            "pushw %%es\n\t"
            "xor %%bx, %%bx\n\t"
            "mov %%bx, %%es\n\t"
            "mov %0, %%es:0x5200\n\t"
            "popw %%es\n\t"
            : : "a"(lfb_lo) : "bx", "memory"
        );
        __asm__ volatile (
            "pushw %%es\n\t"
            "xor %%bx, %%bx\n\t"
            "mov %%bx, %%es\n\t"
            "mov %0, %%es:0x5202\n\t"
            "popw %%es\n\t"
            : : "a"(lfb_hi) : "bx", "memory"
        );
        
        // Speichere restliche Video-Infos (Width, Height, Pitch, BPP)
        // Wir nutzen BX für ES=0, um Registerkonflikte mit den Eingabewerten zu vermeiden.
        __asm__ volatile (
            "pushw %%es\n\t"
            "xor %%bx, %%bx\n\t"
            "mov %%bx, %%es\n\t"
            "mov %0, %%es:0x5204\n\t"
            "popw %%es\n\t"
            : : "a"(best_width) : "bx", "memory"
        );
        __asm__ volatile (
            "pushw %%es\n\t"
            "xor %%bx, %%bx\n\t"
            "mov %%bx, %%es\n\t"
            "mov %0, %%es:0x5206\n\t"
            "popw %%es\n\t"
            : : "a"(best_height) : "bx", "memory"
        );
        __asm__ volatile (
            "pushw %%es\n\t"
            "xor %%bx, %%bx\n\t"
            "mov %%bx, %%es\n\t"
            "mov %0, %%es:0x5208\n\t"
            "popw %%es\n\t"
            : : "a"(best_pitch) : "bx", "memory"
        );
        __asm__ volatile (
            "pushw %%es\n\t"
            "xor %%bx, %%bx\n\t"
            "mov %%bx, %%es\n\t"
            "movb %0, %%es:0x520A\n\t"
            "popw %%es\n\t"
            : : "c"(best_bpp) : "bx", "memory"
        );

    } else {
        puts("VBE Mode not found!\r\n");
        // Fehlerfall: 0x5200 nullen, damit Kernel nicht Müll liest
        __asm__ volatile (
            "pushw %%es; xor %%ax, %%ax; mov %%ax, %%es; movl $0, %%es:0x5200; popw %%es" : : : "ax", "memory"
        );
    }

    __asm__ volatile ("cli"); // Interrupts aus für Protected Mode Switch
    // debug_mode wird nach dem Switch im ASM-Teil ignoriert (nur COM1)
}

// GDT Definition
// 3 Entries: Null, Code (0x08), Data (0x10)
static unsigned long long gdt_table[3] = {
    0x0000000000000000ULL, // Null
    0x00CF9A000000FFFFULL, // Code: Base=0, Limit=4GB, Type=Code(RX), 32bit, 4KB
    0x00CF92000000FFFFULL  // Data: Base=0, Limit=4GB, Type=Data(RW), 32bit, 4KB
};

struct gdt_ptr_struct {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static struct gdt_ptr_struct gdt_ptr = {
    sizeof(gdt_table) - 1,
    (unsigned int)&gdt_table + 0x10000
};

// --- 32-Bit Loader Code ---

void __attribute__((section(".text.entry"))) __attribute__((naked)) preloader_main() {
    __asm__ volatile (
        ".code16\n\t"
        "cli\n\t"
        
        // Setup segments for Real Mode
        // Wir setzen DS=ES=SS=0x1000, damit Offsets relativ zu 0x10000 passen
        "mov $0x1000, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov $0xFFF0, %%sp\n\t" // Stack sicher im Segment platzieren
        
        "call real_mode_logic\n\t"

        // Load GDT
        // Use lgdtl to ensure 32-bit base is used (requires 0x66 prefix in 16-bit mode)
        "lgdtl %0\n\t"

        // Enable Protected Mode (CR0.PE = 1)
        "mov %%cr0, %%eax\n\t"
        "or $1, %%al\n\t"
        "mov %%eax, %%cr0\n\t"

        // Far Jump to 32-bit code (Selector 0x08)
        // Manually encode 32-bit far jump: 0x66 prefix + opcode 0xEA + offset32 + seg16
        ".byte 0x66, 0xea\n\t"
        ".long protected_mode + 0x10000\n\t"
        ".word 0x08\n\t"

        ".code32\n\t"
        "protected_mode:\n\t"
        
        // Initialize Segments with Data Selector (0x10)
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        
        // Setup Stack at 0x90000
        "mov $0x90000, %%esp\n\t"
        
        // --- KERNEL LADEN ---
        // Wir laden 64 Sektoren (32KB) ab LBA 20 nach 0x100000 (1MB)
        // Das ist der "DMA in einem Rutsch" Teil (via rep insw)

        // Wir implementieren den ATA-Treiber hier direkt in Assembly,
        // um Probleme mit 16-Bit C-Compiler-Output im 32-Bit Modus zu vermeiden.

        // 1. ATA Command Setup (LBA 20, Count 64)
        "mov $0x1F6, %%dx\n\t"
        "mov $0xE0, %%al\n\t"     // Drive 0 (Master), LBA Head 0
        "out %%al, %%dx\n\t"

        "mov $0x1F2, %%dx\n\t"
        "mov $64, %%al\n\t"       // Count = 64
        "out %%al, %%dx\n\t"

        "mov $0x1F3, %%dx\n\t"
        "mov $20, %%al\n\t"       // LBA Low = 20
        "out %%al, %%dx\n\t"

        "mov $0x1F4, %%dx\n\t"
        "xor %%al, %%al\n\t"      // LBA Mid = 0
        "out %%al, %%dx\n\t"

        "mov $0x1F5, %%dx\n\t"
        "out %%al, %%dx\n\t"      // LBA High = 0

        "mov $0x1F7, %%dx\n\t"
        "mov $0x20, %%al\n\t"     // Command = Read Sectors
        "out %%al, %%dx\n\t"

        // 2. Data Transfer Loop
        "mov $0x100000, %%edi\n\t" // Zieladresse
        "mov $64, %%ebx\n\t"       // Loop Counter (Sektoren)

        "sector_loop:\n\t"
        // Warte auf DRQ (Bit 3 in Status Register 0x1F7)
        "mov $0x1F7, %%dx\n\t"
        "wait_drq:\n\t"
        "in %%dx, %%al\n\t"
        "test $0x08, %%al\n\t"
        "jz wait_drq\n\t"

        // Lese 256 Wörter (512 Bytes)
        "mov $0x1F0, %%dx\n\t"
        "mov $256, %%ecx\n\t"
        "rep insw\n\t"
        "dec %%ebx\n\t"
        "jnz sector_loop\n\t"

        // Sprung zum Kernel Entry Point (0x100000)
        "mov $0x100000, %%eax\n\t"
        "jmp *%%eax\n\t"

        // Halt
        "halt_loop:\n\t"
        "hlt\n\t"
        "jmp halt_loop\n\t"
        
        : 
        : "m"(gdt_ptr)
        : "eax", "ax", "dx"
    );
}