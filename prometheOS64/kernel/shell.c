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

// --- Sound Blaster 16 (SB16) Wrapper ---
void sb16_beep(unsigned int freq, unsigned int ms) {
    if (freq == 0 || ms == 0) return;
    sb16_play_tone(freq, freq); // Mono-Beep
    sleep(ms);
    sb16_stop();
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
        if (kbd_has_key()) {
            char c = kbd_get_key();
            if (c == 'q' || c == 27) break;
        }
        
        p1_y = mouse_y - 50;
        
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

void shell_main() {
    term_clear();
    
    term_color = 0x00FF00; // Green
    term_puts("PrometheOS Kernel v0.1\n");
    term_puts("Loaded at 0x100000 via PIO-DMA\n");
    term_puts("------------------------------\n");
    term_color = 0xCCCCCC; // Grey

    char cmd_buffer[64];
    int cmd_idx = 0;
    int prev_mx = mouse_x, prev_my = mouse_y;

    while (1) {
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

        render_screen(); 
        
        cmd_idx = 0;
        int prev_blink = 0;
        
        while (1) {
            if (kbd_has_key()) {
                char c = kbd_get_key();

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
                draw_rect(mouse_x, mouse_y, 5, 5, 0xFF0000);
            }
            else {
                int blink = (timer_ticks / 500) % 2;
                int mouse_moved = (mouse_x != prev_mx || mouse_y != prev_my);
                int blink_changed = (blink != prev_blink);

                if (mouse_moved) {
                    restore_mouse_bg(prev_mx, prev_my);
                    prev_mx = mouse_x;
                    prev_my = mouse_y;
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
                    draw_rect(mouse_x, mouse_y, 5, 5, 0xFF0000);
                }
                __asm__ volatile ("hlt");
            }
        }

        if (strcmp(cmd_buffer, "help") == 0) {
            term_puts("Available commands:\n");
            term_puts("  cpuid, memsize, vbe, clear, gfx, sleep\n");
            term_puts("  beep, pong, disk, music, heap, malloc\n");
        } 
        else if (strcmp(cmd_buffer, "cpuid") == 0) {
            unsigned int ebx, ecx, edx;
            __asm__ volatile("cpuid" : "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
            term_puts("CPU Vendor: ");
            char vendor[13];
            *(unsigned int*)&vendor[0] = ebx;
            *(unsigned int*)&vendor[4] = edx;
            *(unsigned int*)&vendor[8] = ecx;
            vendor[12] = 0;
            term_puts(vendor);
            term_puts("\n");
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
            
            kbd_getchar();
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
        else if (cmd_buffer[0] != 0) {
            term_puts("Unknown command.\n");
        }
    }
}