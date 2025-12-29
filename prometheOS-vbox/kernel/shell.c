#include "../include/shell.h"
#include "../include/string.h"
#include "../include/drivers/console.h"
#include "../include/drivers/keyboard.h"
#include "../include/drivers/mouse.h"
#include "../include/drivers/vbe.h"
#include "../include/drivers/sb16.h"
#include "../include/drivers/timer.h"
#include "../include/drivers/ata.h"
#include "../include/scheduler.h"
#include "../include/memory.h"

// --- QEMU Legacy Import Definitions ---
// Diese Definitionen ermöglichen es, die alte Befehlsstruktur 1:1 zu nutzen

typedef enum { T0, T_OPT_STR, T_STR, T_PTR, T_U32, T_U32_U32, T_U32_U32_STR, T_U32_U32_U32 } CmdType;
typedef struct { const char* name; CmdType type; void* fn; } Cmd;

// Externe Referenzen auf die migrierten Funktionen (aus stdlibs, drivers, programs)
// Wenn das Linken fehlschlägt, fehlen die entsprechenden .c Dateien im Makefile!
extern void help(const char*);
extern void msgbox(void); extern void cpuinfo(void);
extern int print_select_list_horizontal(void); extern int print_select_list_vertical(void);
extern void memtest(void); extern void kernel_console_clear(void);
extern void killtimer(void); extern int bell(void); extern void snaketext(void);
extern int ramdisk_test(void); extern int ramdisk_saveimg(void); extern int ramdisk_loadimg(void); extern int tsqlfs_test(void);
extern void db_ls(void); extern void db_cat(const char*); extern void db_saveimg(void); extern void db_loadimg(void);
extern void db_autosave_on(void); extern void db_autosave_off(void); extern void db_info(void);
extern void db_put(uint32_t,uint32_t,const char*); extern void db_get(uint32_t,uint32_t,const char*);
extern void db_rm(const char*); extern void db_edit(const char*); extern void vt_cmd(uint32_t); extern void font(const char*);
extern void restart(void); extern void dobby_cmd(const char*); extern int editor_main(void);
extern void bgademo(void); extern void bga_close(void);
extern void vga_diag_txtforce(void);
extern void dtmf(void); extern void dtmf_bg(const char*); extern void dtmf_stop(void); extern void perftest(void);
extern int setpal(void); extern void snake_main(void); extern void random(void); extern void uptime(void);
extern void hidecursor(void); extern void showcursor(void); extern void print_registers(void); extern void keycodes(void);
extern void exit_kernel(void); extern void loaddisk(void); extern void printascii(void);
extern void cat(void*); extern void run(void*);
extern void cmd_hexdump(uint32_t,uint32_t);
extern void cmd_memset(uint32_t,uint32_t); extern void cmd_memcpy(uint32_t,uint32_t,uint32_t);
extern void searchb(uint32_t,uint32_t,uint32_t);
extern void beep(uint32_t,uint32_t); extern void beepus(uint32_t,uint32_t); extern void beep_bg(uint32_t,uint32_t); extern void beep_stop(void);
extern void beep_sequence(uint32_t,uint32_t,uint32_t);
extern void pcspk_test(uint32_t,uint32_t);
extern void pcspk_sweep3(uint32_t,uint32_t,uint32_t);
extern void pcspk_gliss(uint32_t,uint32_t,uint32_t);
extern void wav_load(const char*); extern void wav_play_cmd(void); extern void wav_stop_cmd(void); extern void wav_play_sb16_cmd(void); extern void playwave_cmd(const char*); extern void sb16_demo(void);
extern void printf(const char*, ...); // Muss aus stdlibs/string.c kommen
extern void searchs(uint32_t,uint32_t,const char*); extern void quit_qemu(void);
extern void palflash(uint32_t); extern void glyphpulse(uint32_t);
extern void net_info(void); extern void netcat_cmd(const char*); extern void net_ping_cmd(const char*);
extern void pingtest_cmd(void); extern void net_dump_regs(void);
extern void taskmgr(void); extern void thread_test(void); extern void sched_info(void);
extern void sched_timeslice(uint32_t); extern void sched_mode(const char*);
extern void now(void); extern void tsrtime(void); extern void thread_kill_cmd(uint32_t);
extern void heartpulse(void); extern void heartspin(void);
extern void palfade_c1(uint32_t,uint32_t,uint32_t); extern void palfade_c2(uint32_t,uint32_t,uint32_t);
extern void printlogo(void); extern int txtmode(void);

// Die mächtige Tabelle aus prometheOS-qemu
static const Cmd CMDS[] = {
    {"help",        T_OPT_STR, help},
    {"msgbox",      T0,        msgbox},
    {"cpuinfo",     T0,        cpuinfo},
    {"memtest",     T0,        memtest},
    {"cls",         T0,        kernel_console_clear},
    {"clear",       T0,        kernel_console_clear},
    {"killtimer",   T0,        killtimer},
    {"bell",        T0,        bell},
    {"snaketext",   T0,        snaketext},
    {"ramdisk_test",T0,        ramdisk_test},
    {"db_ls",       T0,        db_ls},
    {"db_cat",      T_STR,     db_cat},
    {"db_edit",     T_STR,     db_edit},
    {"vt",          T_U32,     vt_cmd},
    {"font",        T_STR,     font},
    {"restart",     T0,        restart},
    {"dobby",       T_STR,     dobby_cmd},
    {"em",          T0,        editor_main},
    {"demo",        T0,        bgademo},
    {"txtmode",     T0,        txtmode},
    {"thread_test", T0,        thread_test},
    {"sched_info",  T0,        sched_info},
    {"now",         T0,        now},
    {"heartpulse",  T0,        heartpulse},
    {"printlogo",   T0,        printlogo},
    {"dtmf",        T0,        dtmf},
    {"snake",       T0,        snake_main},
    {"uptime",      T0,        uptime},
    {"hexdump",     T_U32_U32, cmd_hexdump},
    {"memset",      T_U32_U32, cmd_memset},
    {"memcpy",      T_U32_U32_U32, cmd_memcpy},
    {"beep",        T_U32_U32, beep},
    {"wav_play",    T0,        wav_play_cmd},
    {"sb16_demo",   T0,        sb16_demo},
    {"netinfo",     T0,        net_info},
    {"ping",        T_STR,     net_ping_cmd},
    {"netcat",      T_STR,     netcat_cmd},
    {0, T0, 0}
};

// Hilfsfunktion: Tokenizer für Argumente
static char* next_token(char** pp) {
    char* s = *pp; if (!s) return 0;
    while (*s==' ') s++;
    if (!*s){ *pp = s; return 0; }
    char* start = s;
    while (*s && *s!=' ') s++;
    if (*s) *s++ = 0;
    *pp = s;
    return start;
}

// Hilfsfunktion: U32 Parser
static int parse_u32(char** pp, uint32_t* out){
    char* s = next_token(pp);
    if (!s) return 0;
    *out = (uint32_t)atoi(s); // Nutzt atoi aus string.h
    return 1;
}

static int qemu_shell_active = 0;

// --- MSR Helpers für Long Mode Switch ---
#define MSR_EFER 0xC0000080
#define EFER_LME 0x00000100

static void wrmsr(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static void rdmsr(unsigned int msr, unsigned int *lo, unsigned int *hi) {
    __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

// --- 64-Bit Setup (Identity Paging) ---
// Wir nutzen festen Speicher unter 1MB für die Tabellen, um malloc-Probleme zu vermeiden
// 0x90000: PML4, 0x91000: PDP, 0x92000: PD
void setup_long_mode_paging() {
    unsigned int *pml4 = (unsigned int*)0x90000;
    unsigned int *pdp  = (unsigned int*)0x91000;
    unsigned int *pd   = (unsigned int*)0x92000;

    // Tabellen nullen
    memset(pml4, 0, 4096);
    memset(pdp, 0, 4096);
    memset(pd, 0, 4096);

    // PML4[0] -> PDP
    pml4[0] = (unsigned int)pdp | 3; // Present | Write

    // PDP[0] -> PD
    pdp[0] = (unsigned int)pd | 3;   // Present | Write

    // PD Identity Map (0..2MB) - Wir nutzen 2MB Pages für Einfachheit
    // Wir mappen die ersten 1GB (512 Einträge * 2MB)
    for (int i = 0; i < 512; i++) {
        // Adresse = i * 2MB | Present | Write | HugePage (2MB)
        pd[i] = (i * 0x200000) | 0x83; 
    }

    // CR3 laden
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4));
}

// Wrapper for command table
void beep(uint32_t freq, uint32_t ms) {
    sb16_beep(freq, ms);
}

void beep_sweep(unsigned int start_freq, unsigned int end_freq, unsigned int duration) {
    unsigned int steps = 50;
    unsigned int step_ms = duration / steps;
    if (step_ms == 0) step_ms = 1;
    
    for (unsigned int i = 0; i < steps; i++) {
        unsigned int f = start_freq + (end_freq - start_freq) * i / steps;
        sb16_beep(f, step_ms);
    }
}

// Background Task: Spielt einen Beat
void background_music() {
    while (1) {
        sb16_beep(100, 100); // Kick
        sleep(400);
        sb16_beep(200, 100); // Snare
        sleep(400);
    }
}

void pong() {
    int ball_x = 400, ball_y = 300;
    int ball_dx = 4, ball_dy = 4;
    int p1_y = 250, p2_y = 250;
    int score1 = 0, score2 = 0;
    
    int prev_ball_x = ball_x, prev_ball_y = ball_y;
    int prev_p1_y = p1_y, prev_p2_y = p2_y;
    
    term_clear();
    // Netz einmalig zeichnen
    for(int i=0; i<600; i+=20) draw_rect(398, i, 4, 10, 0x555555);
    
    while (1) {
        unsigned int key = getkey_async();
        if (key) {
            char c = char_from_key(key);
            if (c == 'q' || key == SC_ESC) break;
        }
        
        int mx, my;
        mouse_get_position(&mx, &my);
        p1_y = my - 50;
        
        if (ball_y > p2_y + 50) p2_y += 3;
        if (ball_y < p2_y + 50) p2_y -= 3;
        
        if (p1_y < 0) p1_y = 0; if (p1_y > 500) p1_y = 500;
        if (p2_y < 0) p2_y = 0; if (p2_y > 500) p2_y = 500;
        
        ball_x += ball_dx; ball_y += ball_dy;
        
        if (ball_y <= 0 || ball_y >= 590) ball_dy = -ball_dy;
        
        if (ball_x <= 30 && ball_y >= p1_y && ball_y <= p1_y + 100) {
            ball_dx = -ball_dx; ball_x = 30;
            sb16_beep(440, 60);
        }
        if (ball_x >= 760 && ball_y >= p2_y && ball_y <= p2_y + 100) {
            ball_dx = -ball_dx; ball_x = 760;
            sb16_beep(440, 60);
        }
        
        if (ball_x < 0) { score2++; ball_x = 400; ball_y = 300; ball_dx = 4; sb16_beep(100, 300); }
        if (ball_x > 800) { score1++; ball_x = 400; ball_y = 300; ball_dx = -4; sb16_beep(880, 300); }
        
        draw_rect(prev_ball_x, prev_ball_y, 10, 10, 0x000000);
        draw_rect(20, prev_p1_y, 10, 100, 0x000000);
        draw_rect(770, prev_p2_y, 10, 100, 0x000000);
        
        for(int i=0; i<600; i+=20) draw_rect(398, i, 4, 10, 0x555555);

        draw_rect(300, 30, 200, 20, 0x000000);
        draw_number(350, 30, score1, 0xFFFFFF);
        draw_number(440, 30, score2, 0xFFFFFF);

        draw_rect(20, p1_y, 10, 100, 0xFFFFFF);
        draw_rect(770, p2_y, 10, 100, 0xFFFFFF);
        draw_rect(ball_x, ball_y, 10, 10, 0xFFFFFF);
        
        prev_ball_x = ball_x; prev_ball_y = ball_y;
        prev_p1_y = p1_y; prev_p2_y = p2_y;
        
        sleep(10);
    }
    term_clear();
}

// --- QEMU Dispatcher ---
void qemu_exec(char *input) {
    char* p = input;
    char* cmd_name = next_token(&p);
    if (!cmd_name) return;

    if (strcmp(cmd_name, "exit") == 0) {
        qemu_shell_active = 0;
        return;
    }

    for (int i=0; CMDS[i].name; i++) {
        if (strcmp(cmd_name, CMDS[i].name) == 0) {
            switch (CMDS[i].type) {
                case T0: ((void(*)(void))CMDS[i].fn)(); break;
                case T_STR: {
                    char* a = next_token(&p);
                    ((void(*)(const char*))CMDS[i].fn)(a ? a : "");
                } break;
                case T_OPT_STR: {
                    char* a = next_token(&p);
                    ((void(*)(const char*))CMDS[i].fn)(a ? a : "");
                } break;
                case T_U32: {
                    uint32_t v; parse_u32(&p, &v);
                    ((void(*)(uint32_t))CMDS[i].fn)(v);
                } break;
                case T_U32_U32: {
                    uint32_t v1, v2; parse_u32(&p, &v1); parse_u32(&p, &v2);
                    ((void(*)(uint32_t, uint32_t))CMDS[i].fn)(v1, v2);
                } break;
                // Weitere Typen hier implementieren...
                default: term_puts("CmdType not fully implemented in VBox adapter.\n"); break;
            }
            return;
        }
    }
    term_puts("Unknown QEMU command: "); term_puts(cmd_name); term_puts("\n");
}

void shell_main() {
    term_clear();
    
    term_color = 0x00FF00; // Green
    term_puts("PrometheOS Kernel v0.1\n");
    term_puts("Loaded at 0x100000 via PIO-DMA\n");
    term_puts("------------------------------\n");
    term_color = 0xCCCCCC; // Grey

    char cmd_buffer[64];
    int cmd_idx = 0;
    int mx, my;
    mouse_get_position(&mx, &my);
    int prev_mx = mx, prev_my = my;

    while (1) {
        // Prompt anpassen je nach Modus
        if (qemu_shell_active) {
            term_color = 0xFF0000;
            term_puts("\nQEMU-MODE> ");
            term_color = 0xCCCCCC;
        } else {
        // Bunter Prompt
        term_color = 0x00FF00; // User (Green)
        term_puts("\nsimon");
        term_color = 0xFFFFFF; // @ (White)
        term_puts("@");
        term_color = 0x5555FF; // Host (Blue)
        term_puts("prometheos");
        term_color = 0xFFFFFF; // : (White)
        term_puts(":");
        term_color = 0x00FFFF; // Path (Cyan)
        term_puts("/> ");
        term_color = 0xCCCCCC; // Reset
        }

        render_screen(); 
        
        cmd_idx = 0;
        int prev_blink = 0;
        
        while (1) {
            unsigned int key = getkey_async();
            if (key) {
                char c = char_from_key(key);

                if (c == '\n') {
                    term_putc('\n');
                    cmd_buffer[cmd_idx] = 0;
                    render_screen();
                    break;
                } else if (c == '\b') {
                    if (cmd_idx > 0) {
                        term_putc('\b');
                        cmd_idx--;
                    }
                } else if (c) {
                    term_putc(c);
                    if (cmd_idx < 63) cmd_buffer[cmd_idx++] = c;
                }
                render_screen();
                draw_rect(term_col * 9, term_row * 16 + 14, 8, 2, term_color);
                mouse_get_position(&mx, &my);
                draw_rect(mx, my, 5, 5, 0xFF0000);
            }
            else {
                int blink = (timer_ticks / 500) % 2;
                mouse_get_position(&mx, &my);
                int mouse_moved = (mx != prev_mx || my != prev_my);
                int blink_changed = (blink != prev_blink);

                if (mouse_moved) {
                    restore_mouse_bg(prev_mx, prev_my);
                    prev_mx = mx;
                    prev_my = my;
                }

                if (blink_changed) {
                    if (blink) {
                        draw_rect(term_col * 9, term_row * 16 + 14, 8, 2, term_color);
                    } else {
                        unsigned short entry = text_buffer[term_row * 80 + term_col];
                        draw_char(term_col * 9, term_row * 16, entry & 0xFF, term_color);
                    }
                    prev_blink = blink;
                }
                
                if (mouse_moved || blink_changed) {
                    draw_rect(mx, my, 5, 5, 0xFF0000);
                }
                __asm__ volatile ("hlt");
            }
        }

        // Dispatcher Entscheidung
        if (qemu_shell_active) {
            qemu_exec(cmd_buffer);
        }
        else if (strcmp(cmd_buffer, "help") == 0) {
            term_puts("Available commands:\n");
            term_puts("  cpuid, memsize, vbe, clear, gfx, sleep\n");
            term_puts("  beep, pong, disk, music, heap, malloc, sb16, longmode, switch64\n");
            term_puts("  qemu (Enter Legacy Mode)\n");
        } 
        else if (strcmp(cmd_buffer, "cpuid") == 0) {
            unsigned int eax, ebx, ecx, edx;
            
            // 1. Vendor ID (Leaf 0)
            __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
            term_puts("Vendor: ");
            char vendor[13];
            *(unsigned int*)&vendor[0] = ebx;
            *(unsigned int*)&vendor[4] = edx;
            *(unsigned int*)&vendor[8] = ecx;
            vendor[12] = 0;
            term_puts(vendor);
            term_puts("\n");

            // 2. Version Info: Family, Model, Stepping (Leaf 1)
            __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
            unsigned int stepping = eax & 0xF;
            unsigned int model = (eax >> 4) & 0xF;
            unsigned int family = (eax >> 8) & 0xF;
            unsigned int type = (eax >> 12) & 0x3;
            unsigned int ext_model = (eax >> 16) & 0xF;
            unsigned int ext_family = (eax >> 20) & 0xFF;

            if (family == 6 || family == 15) model += (ext_model << 4);
            if (family == 15) family += ext_family;

            term_puts("Family: "); term_putdec(family);
            term_puts(" Model: "); term_putdec(model);
            term_puts(" Step: "); term_putdec(stepping);
            term_puts(" Type: "); term_putdec(type);
            term_puts("\n");

            // 3. Brand String (Extended Function 0x80000002 - 0x80000004)
            __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));
            if (eax >= 0x80000004) {
                char brand[49];
                unsigned int *p = (unsigned int*)brand;
                // Drei Aufrufe für je 16 Bytes des Strings
                __asm__ volatile("cpuid" : "=a"(p[0]), "=b"(p[1]), "=c"(p[2]), "=d"(p[3]) : "a"(0x80000002));
                __asm__ volatile("cpuid" : "=a"(p[4]), "=b"(p[5]), "=c"(p[6]), "=d"(p[7]) : "a"(0x80000003));
                __asm__ volatile("cpuid" : "=a"(p[8]), "=b"(p[9]), "=c"(p[10]), "=d"(p[11]) : "a"(0x80000004));
                brand[48] = 0;
                
                // Führende Leerzeichen überspringen für schönere Ausgabe
                char *s = brand;
                while (*s == ' ') s++;
                term_puts("Brand: "); term_puts(s); term_puts("\n");
            }
        }
        else if (strcmp(cmd_buffer, "memsize") == 0) {
            unsigned int *mem_kb = (unsigned int *)0x5100;
            term_puts("System RAM: "); term_putdec(*mem_kb); term_puts(" KB\n");
        }
        else if (strcmp(cmd_buffer, "vbe") == 0) {
            struct vbe_info_raw {
                char signature[4];
                unsigned short version;
                unsigned int oem_ptr;
                unsigned int capabilities;
                unsigned int video_mode_ptr;
                unsigned short total_memory;
            } __attribute__((packed)) *vbe = (void*)0x5000;

            if (vbe->signature[0] == 'V' && vbe->signature[1] == 'E') {
                term_puts("VBE Version: "); term_puthex(vbe->version); term_puts("\n");
                term_puts("Video Memory: "); term_putdec(vbe->total_memory * 64); term_puts(" KB\n");
                term_puts("Capabilities: "); term_puthex(vbe->capabilities); term_puts("\n");
            } else {
                term_puts("Error: No VBE Info found at 0x5000.\n");
            }
        }
        else if (strcmp(cmd_buffer, "clear") == 0) {
            term_clear();
        }
        else if (strcmp(cmd_buffer, "gfx") == 0) {
            term_puts("Drawing Test Pattern... Press any key to return.\n");
            render_screen();
            for(int y=100; y<500; y++)
                for(int x=100; x<700; x++)
                    put_pixel(x, y, ((x*255/800)<<16) | ((y*255/600)<<8) | ((x+y)*255/1400));
            
            draw_rect(150, 150, 100, 50, 0xFF0000); // Rotes Rechteck
            draw_rect(300, 200, 50, 100, 0x0000FF); // Blaues Rechteck
            draw_circle(500, 300, 50, 0xFFFF00);    // Gelber Kreis
            draw_circle(600, 400, 30, 0xFF00FF);    // Magenta Kreis
            
            // Verschachtelte Boxen
            for(int i=0; i<5; i++) {
                 draw_rect(350 + i*10, 350 + i*10, 100 - i*20, 100 - i*20, 0xFFFFFF - i*0x303030);
            }
            
            getkey();
            term_clear();
        }
        else if (strcmp(cmd_buffer, "sleep") == 0) {
            term_puts("Sleeping 1s...\n");
            sleep(1000);
            term_puts("Woke up!\n");
        }
        else if (cmd_buffer[0] == 'b' && cmd_buffer[1] == 'e' && cmd_buffer[2] == 'e' && cmd_buffer[3] == 'p') {
            char *p = cmd_buffer + 4;
            while (*p == ' ') p++;
            int freq = atoi(p);
            while (*p >= '0' && *p <= '9') p++; while (*p == ' ') p++;
            int ms = atoi(p);
            if (freq > 0 && ms > 0) sb16_beep(freq, ms);
        }
        else if (strcmp(cmd_buffer, "pong") == 0) {
            pong();
        }
        else if (strcmp(cmd_buffer, "disk") == 0) {
            unsigned char sector[512];
            ata_read_sector(0, sector);
            term_puts("MBR Read OK.\n");
        }
        else if (strcmp(cmd_buffer, "music") == 0) {
            task_create(background_music);
        }
        else if (strcmp(cmd_buffer, "heap") == 0) {
            print_memory_status();
        }
        else if (strcmp(cmd_buffer, "malloc") == 0) {
            term_puts("Test allocation (1KB)...\n");
            void* p = malloc(1024);
            if (p) {
                term_puts("Address: 0x"); term_puthex((unsigned int)p); term_puts("\n");
                free(p);
            } else {
                term_puts("Allocation failed.\n");
            }
        }
        else if (cmd_buffer[0] == 's' && cmd_buffer[1] == 'b' && cmd_buffer[2] == '1' && cmd_buffer[3] == '6') {
            char *p = cmd_buffer + 4;
            while (*p == ' ') p++;
            
            if (*p == 0) {
                term_puts("Testing SB16 (440Hz)... ");
                sb16_beep(440, 500);
                term_puts("Done.\n");
            } else {
                // Parse: freqL freqR duration
                int f1 = atoi(p);
                while (*p >= '0' && *p <= '9') p++; while (*p == ' ') p++;
                int f2 = atoi(p);
                while (*p >= '0' && *p <= '9') p++; while (*p == ' ') p++;
                int ms = atoi(p);

                // Logik: 
                // 2 Args (f1, f2) -> Mono Beep: Freq=f1, Dauer=f2
                // 3 Args (f1, f2, ms) -> Binaural: Links=f1, Rechts=f2, Dauer=ms
                if (ms == 0 && f2 > 0) { ms = f2; f2 = f1; } // Fallback auf Mono-Syntax
                if (ms == 0) ms = 500; // Default Dauer
                if (f2 == 0) f2 = f1;  // Default Mono

                term_puts("SB16: L="); term_putdec(f1); term_puts("Hz R="); term_putdec(f2); 
                term_puts("Hz T="); term_putdec(ms); term_puts("ms\n");
                
                sb16_play_tone(f1, f2);
                sleep(ms);
                sb16_stop();
            }
        }
        else if (strcmp(cmd_buffer, "longmode") == 0) {
            unsigned int eax, edx;
            __asm__ volatile("cpuid" : "=a"(eax) : "a"(0x80000000));
            if (eax < 0x80000001) {
                term_puts("Long Mode: Not supported (CPUID ext missing)\n");
            } else {
                __asm__ volatile("cpuid" : "=d"(edx) : "a"(0x80000001));
                if (edx & (1 << 29)) {
                    term_puts("Long Mode: Supported (x86-64 available)\n");
                } else {
                    term_puts("Long Mode: Not supported (Bit 29 missing)\n");
                }
            }
        }
        else if (strcmp(cmd_buffer, "switch64") == 0) {
            term_puts("Preparing 64-bit transition...\n");
            
            // 1. Interrupts aus (wichtig!)
            __asm__ volatile("cli");
            
            // 2. PAE aktivieren (CR4 Bit 5)
            unsigned int cr4;
            __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
            cr4 |= (1 << 5);
            __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
            
            // 3. Page Tables laden (CR3)
            setup_long_mode_paging();
            
            // 4. EFER LME setzen (MSR 0xC0000080 Bit 8)
            unsigned int lo, hi;
            rdmsr(MSR_EFER, &lo, &hi);
            lo |= EFER_LME;
            wrmsr(MSR_EFER, lo, hi);
            
            // 5. Paging aktivieren (CR0 Bit 31)
            unsigned int cr0;
            __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
            cr0 |= (1 << 31);
            __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
            
            term_puts("Success! CPU is now in Long Mode (Compatibility Mode).\n");
            term_puts("WARNING: Do not enable interrupts without new IDT!\n");
        }
        else if (strcmp(cmd_buffer, "qemu") == 0) {
            qemu_shell_active = 1;
            term_puts("Entering QEMU Legacy Mode. Type 'exit' to return.\n");
        }
        else if (cmd_buffer[0] != 0) {
            term_puts("Unknown command.\n");
        }
    }
}