#include "../cpu/idt.h"
#include "../cpu/isr.h"
#include "../cpu/timer.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/ports.h"
#include "../drivers/video.h"
#include "../drivers/hdd.h"
#include "../stdlibs/string.h"
#include "../stdlibs/file.h"
#include "../stdlibs/tsqlfs.h"
#include "../stdlibs/homebrewdb.h"
#include "../programs/editor.h"
#include "../cpu/jmpbuf.h"
#include "../cpu/cpuinfo.h"

#include "kernel.h"
#include "util.h"
#include "time.h"
#include "ui.h"
#include "rtc.h"

#define KERNEL_PROMPT_CHAR 0x10
#define KERNEL_PROMPT_COLOR (FG_RED | BG_BLACK)

jmp_buf g_jmpKernelMain;

bool g_bKernelShouldStop = false;
// When a command prints its own prompt (e.g., demo), skip the default prompt once
int g_skip_prompt_once = 0;
bool g_bKernelInitialised = false;
int g_iKernelRevnum = REVISION_NUMBER;
unsigned char* g_strKernelRevdate = REVISION_DATE;

// ---- Command dispatch types and table (global) -----------------------------
typedef enum { T0, T_OPT_STR, T_STR, T_PTR, T_U32, T_U32_U32, T_U32_U32_STR, T_U32_U32_U32 } CmdType;
typedef struct { const char* name; CmdType type; void* fn; } Cmd;

// Forward declarations for command functions (minimal signatures)
extern void help(const char*);
extern void msgbox(void); extern void cpuinfo(void);
extern int print_select_list_horizontal(void); extern int print_select_list_vertical(void);
extern void memtest(void); extern void kernel_console_clear(void);
extern void killtimer(void); extern int bell(void); extern void snaketext(void);
extern int ramdisk_test(void); extern int ramdisk_saveimg(void); extern int ramdisk_loadimg(void); extern int tsqlfs_test(void);
extern void db_ls(void); extern void db_cat(const char*); extern void db_saveimg(void); extern void db_loadimg(void);
extern void db_autosave_on(void); extern void db_autosave_off(void); extern void db_info(void);
extern void db_put(uint32_t,uint32_t,const char*); extern void db_get(uint32_t,uint32_t,const char*);
extern void db_rm(const char*); extern void db_edit(const char*); extern void font(const char*);
extern void restart(void); extern void dobby(const char*); extern int editor_main(void);
extern void bgademo(void); extern void sb16demo(void);
extern void dtmf(void); extern void dtmf_bg(const char*); extern void dtmf_stop(void); extern void perftest(void);
extern int setpal(void); extern void snake_main(void); extern void random(void); extern void uptime(void);
extern void hidecursor(void); extern void showcursor(void); extern void print_registers(void); extern void keycodes(void);
extern void exit(void); extern void loaddisk(void); extern void printascii(void);
extern void cat(void*); extern void run(void*);
extern void cmd_hexdump(uint32_t,uint32_t);
extern void cmd_memset(uint32_t,uint32_t); extern void cmd_memcpy(uint32_t,uint32_t,uint32_t);
extern void searchb(uint32_t,uint32_t,uint32_t);
extern void beep(uint32_t,uint32_t); extern void beepus(uint32_t,uint32_t); extern void beep_bg(uint32_t,uint32_t); extern void beep_stop(void);
extern void beep_sequence(uint32_t,uint32_t,uint32_t);
extern void searchs(uint32_t,uint32_t,const char*); extern void quit_qemu(void);
extern void palflash(uint32_t); extern void glyphpulse(uint32_t);
extern void thread_test(void);
extern void sched_info(void);
extern void sched_timeslice(uint32_t);
extern void sched_mode(const char*);
extern void now(void);
extern void tsrtime(void);
extern void thread_kill_cmd(uint32_t);
extern void heartpulse(void);
extern void heartspin(void);
extern void palfade_c1(uint32_t,uint32_t,uint32_t);
extern void palfade_c2(uint32_t,uint32_t,uint32_t);
extern void printlogo(void);

extern int ide_main(void);

static const Cmd CMDS[] = {
    {"help",        T_OPT_STR, help},
    {"msgbox",      T0,        msgbox},
    {"cpuinfo",     T0,        cpuinfo},
    {"pl",          T0,        print_select_list_horizontal},
    {"pv",          T0,        print_select_list_vertical},
    {"memtest",     T0,        memtest},
    {"cls",         T0,        kernel_console_clear},
    {"clear",       T0,        kernel_console_clear},
    {"clr",         T0,        kernel_console_clear},
    {"rst",         T0,        kernel_console_clear},
    {"reset",       T0,        kernel_console_clear},
    {"killtimer",   T0,        killtimer},
    {"bell",        T0,        bell},
    {"snaketext",   T0,        snaketext},
    {"ramdisk_test",T0,        ramdisk_test},
    {"ramdisk_saveimg",T0,     ramdisk_saveimg},
    {"ramdisk_loadimg",T0,     ramdisk_loadimg},
    {"tsqlfs_test", T0,        tsqlfs_test},
    {"db_ls",       T0,        db_ls},
    {"db_cat",      T_STR,     db_cat},
    {"db_saveimg",  T0,        db_saveimg},
    {"db_loadimg",  T0,        db_loadimg},
    {"db_autosave_on", T0,     db_autosave_on},
    {"db_autosave_off",T0,     db_autosave_off},
    {"db_info",     T0,        db_info},
    {"db_put",      T_U32_U32_STR, db_put},
    {"db_get",      T_U32_U32_STR, db_get},
    {"db_rm",       T_STR,     db_rm},
    {"db_edit",     T_STR,     db_edit},
    {"font",        T_STR,     font},
    {"restart",     T0,        restart},
    {"dobby",       T_STR,     dobby},
    {"em",          T0,        editor_main},
    {"demo",        T0,        bgademo},
    {"sb16",        T0,        sb16demo},
    {"thread_test", T0,        thread_test},
    {"sched_info",  T0,        sched_info},
    {"sched_timeslice", T_U32, sched_timeslice},
    {"sched_mode",  T_STR,     sched_mode},
    {"now",         T0,        now},
    {"tsrtime",     T0,        tsrtime},
    {"thread_kill", T_U32,     thread_kill_cmd},
    {"heartpulse",  T0,        heartpulse},
    {"heartspin",   T0,        heartspin},
    {"palfade_c1",  T_U32_U32_U32, palfade_c1},
    {"palfade_c2",  T_U32_U32_U32, palfade_c2},
    {"printlogo",  T0,        printlogo},
    {"dtmf",        T0,        dtmf},
    {"dtmf_bg",     T_STR,     dtmf_bg},
    {"dtmf_stop",   T0,        dtmf_stop},
    {"perftest",    T0,        perftest},
    {"setpal",      T0,        setpal},
    {"snake_main",  T0,        snake_main},
    {"random",      T0,        random},
    {"uptime",      T0,        uptime},
    {"hidecursor",  T0,        hidecursor},
    {"showcursor",  T0,        showcursor},
    {"print_registers",T0,     print_registers},
    {"keycodes",    T0,        keycodes},
    {"exit",        T0,        exit},
    {"loaddisk",    T0,        loaddisk},
    {"printascii",  T0,        printascii},
    {"cat",         T_PTR,     cat},
    {"run",         T_PTR,     run},
    {"hexdump",     T_U32_U32, cmd_hexdump},
    {"memset",      T_U32_U32, cmd_memset},
    {"memcpy",      T_U32_U32_U32, cmd_memcpy},
    {"searchb",     T_U32_U32_U32, searchb},
    {"beep",        T_U32_U32, beep},
    {"beepus",      T_U32_U32, beepus},
    {"beep_bg",     T_U32_U32, beep_bg},
    {"beep_stop",   T0,        beep_stop},
    {"beep_sequence",T_U32_U32_U32, beep_sequence},
    {"printf",      T_STR,     printf},
    {"searchs",     T_U32_U32_STR, searchs},
    {"quit_qemu",   T0,        quit_qemu},
    {"palflash",    T_U32,     palflash},
    {"glyphpulse",  T_U32,     glyphpulse}
};


int kernel_console_program();

#include "../cpu/jmpbuf.h"
jmp_buf kernel_env;

#define REVISION_NAME "PrometheOS"

void kernel_main() {
    setjmp(&kernel_env);
	g_jmpKernelMain =  kernel_env;
    set_color(WHITE_ON_BLACK);
    clear_screen();
    printf("start_kernel @ 0x%p\n", &kernel_main);
    debug_puts("start_kernel via port 0xe9\n");

	unsigned char str[80];
    sprintf(str, "%s 0.%d (%s)\n", REVISION_NAME, (void*)g_iKernelRevnum, (void*)g_strKernelRevdate);
    printf(str);

    if(! g_bKernelInitialised) {
        print_string("Installing interrupt service routines (ISRs).\n");
        isr_install();
        print_string("Enabling external interrupts.\n");
        asm volatile("sti");
        print_string("Initializing keyboard (IRQ 1).\n");
        init_keyboard();
        print_string("Initializing memory managment.\n");
        init_memory();
        init_dynamic_mem();

        print_string("A20 Line was activated by the MBR.\n");
        // enable_a20_line();

        print_string("Initializing timer millisecond.\n");
        init_timer(1000);

        print_string("Initializing PS/2 mouse interface.\n");
        mouse_install();

        print_string("Initializing random number generator.\n");;
        init_random();

        print_string("Initializing FPU.\n");;
        fpu_init();

        print_string("Initializing Perfomance Counter.\n");;
		perf_init();
        print_string("Seeding wall clock from RTC.\n");
        time_init_with_rtc();

        // Load HomebrewDB from DB_START_LBA (after kernel region) and enable autosave
        hbdb_load_image(16514);
        hbdb_set_image_lba(16514);
        hbdb_set_autosave(1);

		g_bKernelInitialised = true;
    }

    kernel_console_key_buffer[0] = '\0';

    // Do not bulk-read the disk into low memory here: it corrupts the heap
    // used by our libc-style allocator (memory[] in stdlibs/memory.c).
    // Any such tests should use a dedicated scratch buffer and stay clear of
    // the BSS region.
    {
        unsigned char oldc = get_color();
        set_color(KERNEL_PROMPT_COLOR);
        printf("%c", KERNEL_PROMPT_CHAR);
        set_color(oldc);
        printf(" ");
    }

    while(!g_bKernelShouldStop) {
        kernel_console_program();
    }

end_of_kernel:
    print_nl();
    printf("Wow, we should get here...\nExiting...\nStopping the CPU...\n");
    // Fully stop CPU: disable interrupts, then halt forever.
    // With IF=1, HLT only sleeps until the next interrupt (PIT/keyboard),
    // which is why execution seemed to continue after HLT.
    asm volatile("cli");
    for(;;){ asm volatile("hlt"); }
}

void kernel_console_execute_command(char *input) {
    int cursor = get_cursor();
    if (strcmp(input, "") == 0) { goto none; }

    // --- Table based dispatcher --- (see global CMDS[] above)

    // Parse command and args
    // Quote-aware tokenizer for string arguments
    auto char* next_token(char** pp){
        char* s = *pp; if (!s) return 0;
        while (*s==' ') s++;
        if (!*s){ *pp = s; return 0; }
        char* start = s;
        char q = 0;
        if (*s=='"' || *s=='\'') { q = *s; s++; start = s; }
        char* out = start;
        if (q){
            while (*s){
                if (*s=='\\' && s[1]){ s++; *out++ = *s++; continue; }
                if (*s==q){ s++; break; }
                *out++ = *s++;
            }
        } else {
            while (*s && *s!=' ') { *out++ = *s++; }
        }
        *out = '\0';
        while (*s==' ') s++;
        *pp = s;
        return start;
    }
    // Simple numeric parser that does not modify the buffer
    auto int parse_u32(char** pp, uint32_t* out){
        char* s = *pp; if (!s) return 0; while (*s==' ') s++; if (!*s) { *pp = s; return 0; }
        // refuse quoted for numeric; let user quote only for strings
        if (*s=='"' || *s=='\'') return 0;
        char* end = 0; unsigned long v = strtoul(s, &end, 0);
        if (end == s) return 0; // no digits
        *out = (uint32_t)v;
        while (*end==' ') end++;
        *pp = end; return 1;
    }
    char* p = input;
    while (*p==' ') p++;
    char cmd[32]; int ci=0; while (*p && *p!=' ' && ci< (int)sizeof(cmd)-1) cmd[ci++]=*p++;
    cmd[ci]='\0';
    while (*p==' ') p++;

    int handled = 0;
    for (unsigned int i=0;i<sizeof(CMDS)/sizeof(CMDS[0]);i++){
        if (strcmp(cmd, CMDS[i].name)==0){
            handled = 1;
            switch (CMDS[i].type){
                case T0: ((void(*)(void))CMDS[i].fn)(); break;
                case T_OPT_STR: { const char* s = (*p)? next_token(&p) : ""; ((void(*)(const char*))CMDS[i].fn)(s ? s : ""); } break;
                case T_STR: { char* a = next_token(&p); if(!a){ printf("Invalid parameters for %s\n", CMDS[i].name); help(CMDS[i].name); break; } ((void(*)(const char*))CMDS[i].fn)(a); } break;
                case T_U32: { uint32_t v; if(!parse_u32(&p,&v)){ printf("Invalid parameters for %s\n", CMDS[i].name); help(CMDS[i].name); break;} ((void(*)(uint32_t))CMDS[i].fn)(v);} break;
                case T_PTR: { uint32_t v; if(!parse_u32(&p,&v)){ printf("Invalid parameters for %s\n", CMDS[i].name); help(CMDS[i].name); break;} ((void(*)(void*))CMDS[i].fn)((void*)v); } break;
                case T_U32_U32: { uint32_t v1,v2; if(!parse_u32(&p,&v1) || !parse_u32(&p,&v2)){ printf("Invalid parameters for %s\n", CMDS[i].name); help(CMDS[i].name); break;} ((void(*)(uint32_t,uint32_t))CMDS[i].fn)(v1,v2);} break;
                case T_U32_U32_STR: { uint32_t v1,v2; if(!parse_u32(&p,&v1) || !parse_u32(&p,&v2)){ printf("Invalid parameters for %s\n", CMDS[i].name); help(CMDS[i].name); break;} char* c = next_token(&p); if(!c){ printf("Invalid parameters for %s\n", CMDS[i].name); help(CMDS[i].name); break;} ((void(*)(uint32_t,uint32_t,const char*))CMDS[i].fn)(v1,v2,c);} break;
                case T_U32_U32_U32: { uint32_t v1,v2,v3; if(!parse_u32(&p,&v1) || !parse_u32(&p,&v2) || !parse_u32(&p,&v3)){ printf("Invalid parameters for %s\n", CMDS[i].name); help(CMDS[i].name); break;} ((void(*)(uint32_t,uint32_t,uint32_t))CMDS[i].fn)(v1,v2,v3);} break;
            }
            break;
        }
    }

    if (!handled){
        printf("%s ", KERNEL_PROMPT_UNKNOWN_COMMAND, input);
        printf("%s\n", input);
    }
    if (!g_skip_prompt_once) printf("%c ", KERNEL_PROMPT_CHAR);
    else g_skip_prompt_once = 0;
    return;
none:
    set_cursor(cursor - 156);
    return;
}

unsigned int g_isExtendedKey = 0;
unsigned int isExtendedKey(unsigned int key) {
    return key >> 8 == 0xE0;
}

// Console program one
int kernel_console_program() {
    while (1) {
        unsigned int key = getkey();
        unsigned int chr = char_from_key(key);
        
        if (key == SC_BACKSPACE) {
            if (backspace(kernel_console_key_buffer)) {
                print_backspace();
            }
        } else if (key == SC_ENTER) {
            clear_cursor();
            print_nl();
            kernel_console_execute_command(kernel_console_key_buffer);
            kernel_console_key_buffer[0] = '\0';
        } 
        else if (key == SC_F1 && is_key_pressed(SC_LEFT_CTRL)) {
            editor_main();
            printf("%c ", KERNEL_PROMPT_CHAR);
        }
        else if ((key == SC_PAGEUP) || (isExtendedKey(key) && ((key & 0xFF) == SC_PAGEUP))) {
            console_page_up();
        }
        else if ((key == SC_PAGEDOWN) || (isExtendedKey(key) && ((key & 0xFF) == SC_PAGEDOWN))) {
            console_page_down();
        }
        else {
            append(kernel_console_key_buffer, chr);
            char str[2] = {chr, '\0'};
            print_string(str);
        }

    }
}
