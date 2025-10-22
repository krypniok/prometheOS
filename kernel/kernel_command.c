#include <stddef.h>

#include "kernel.h"
#include "time.h"

#include "../drivers/keyboard.h"
#include "../drivers/display.h"
#include "../drivers/hdd.h"
#include "../drivers/sb16.h"
#include "../cpu/jmpbuf.h"
#include "../cpu/cpuinfo.h"
#include "../kernel/perf.h"
#include "rtc.h"
#include "../stdlibs/memory.h"
#include "../stdlibs/string.h"
#include "../stdlibs/tsqlfs.h"
#include "../stdlibs/homebrewdb.h"
#include "../stdlibs/font_psf.h"
#include "../programs/editor.h"
#include "../kernel/conio.h"
#include "thread.h"
#include "rtc.h"

int dobby(const char* filename);

void vt_cmd(uint32_t index);

static char* g_dobby_thread_args[MAX_THREADS];

static void dobby_thread_worker(void) {
    int tid = thread_current_id();
    char* filename = NULL;
    if (tid >= 0 && tid < MAX_THREADS) {
        // wait until launcher populates our slot
        while ((filename = g_dobby_thread_args[tid]) == NULL) {
            sleep_us(500);
        }
        g_dobby_thread_args[tid] = NULL;
    }
    if (!filename) {
        printf("[dobby] thread %d missing filename\n", tid);
        return;
    }

    printf("[dobby] thread %d running %s\n", tid, filename);
    int rc = dobby(filename);
    printf("[dobby] thread %d finished (%d)\n", tid, rc);
    free(filename);
}

static int dobby_launch_thread(const char* filename) {
    if (!filename || !filename[0]) {
        printf("usage: dobby <name.c>\n");
        return -1;
    }
    size_t len = strlen(filename);
    if (len >= 240) {
        printf("dobby: name too long\n");
        return -1;
    }
    char* copy = (char*)malloc(len + 1);
    if (!copy) {
        printf("dobby: alloc failed\n");
        return -1;
    }
    memcpy(copy, filename, len + 1);

    int tid = thread_create(dobby_thread_worker);
    if (tid < 0) {
        printf("dobby: thread create failed\n");
        free(copy);
        return -1;
    }
    if (tid >= MAX_THREADS) {
        printf("dobby: invalid thread id\n");
        free(copy);
        return -1;
    }

    g_dobby_thread_args[tid] = copy;
    printf("[dobby] spawned thread %d for %s\n", tid, filename);
    return tid;
}

void dobby_cmd(const char* filename) {
    int tid = dobby_launch_thread(filename);
    if (tid >= 0) {
        // give the interpreter a chance to start before returning to console
        thread_yield();
    }
}

void vt_cmd(uint32_t index) {
    if (index == 0 || index > 12) {
        printf("vt: use 1..12\n");
        return;
    }
    kernel_request_vt_switch((int)(index - 1));
}


extern bool g_bKernelShouldStop;
extern jmp_buf g_jmpKernelMain;

typedef struct {
    const char *name;
    const char *desc;
} help_entry;

static const help_entry g_help_entries[] = {
    {"help", "List kernel commands"},
    {"cpuinfo", "Show CPU information"},
    {"msgbox", "Show message box"},
    {"pl", "Print list"},
    {"pv", "Print vertical list"},
    {"memtest", "Run memory test"},
    {"cls/clear/clr/rst/reset", "Clear screen"},
    {"killtimer", "Stop PIT timer"},
    {"bell", "Play bell"},
    {"snaketext", "Snake text demo"},
    {"ramdisk_test", "Test RAM disk"},
    {"db_ls", "List DB files"},
    {"db_cat <name>", "Print DB file"},
    {"db_loadimg", "Load DB from 1MiB"},
    {"db_saveimg", "Save DB to 1MiB"},
    {"db_autosave_on", "Enable save-on-close"},
    {"db_autosave_off", "Disable save-on-close"},
    {"db_info", "Show FS row count"},
    {"db_put <addr> <len> <name>", "Put memory as file"},
    {"db_get <addr> <max> <name>", "Get file to memory"},
    {"db_rm <name>", "Remove file"},
    {"db_edit <name>", "Edit DB text file"},
    {"vt <n>", "Switch virtual console (1-12)"},
    {"restart", "Restart kernel"},
    {"dobby <name>", "Run Little C from DB"},
    {"em", "Text editor"},
    {"demo", "BGA demo"},
    {"txtmode", "Restore VGA 80x25 text mode"},
    {"thread_test", "Spawn two demo threads (coop/preempt)"},
    {"tsrtime", "Start clock thread top-right"},
    {"thread_kill <id>", "Signal a thread to stop"},
    {"taskmgr", "Show running threads"},
    {"heartpulse", "Start pulsing hearts (ASCII code)"},
    {"sched_info", "Show scheduler timeslice and state"},
    {"sched_timeslice <ms>", "Set scheduler timeslice (1..1000 ms)"},
    {"sched_mode <preempt|coop>", "Switch scheduler mode"},
    {"perftest", "Test rdtsc/micros accuracy"},
    {"font <name.psf>", "Load PSF font (256 glyphs)"},
    {"dtmf", "Generate DTMF tones"},
    {"dtmf_bg <seq>", "DTMF in background"},
    {"dtmf_stop", "Stop background DTMF"},
    {"setpal", "Change palette"},
    {"snake_main", "Snake game"},
    {"random", "Random numbers"},
    {"uptime", "Show uptime"},
    {"hidecursor", "Hide cursor"},
    {"showcursor", "Show cursor"},
    {"print_registers", "Dump registers"},
    {"keycodes", "Keyboard scancodes"},
    {"exit", "Leave kernel"},
    {"loaddisk", "Load disk"},
    {"printascii", "ASCII table"},
    {"cat <addr>", "Print memory"},
    {"hexviewer <addr>", "Hex viewer"},
    {"run <addr>", "Jump to address"},
    {"hexdump <addr> <len>", "Hex dump"},
    {"memset <addr> <len>", "Fill memory"},
    {"memcpy <src> <dst> <len>", "Copy memory"},
    {"searchb <addr> <len> <b>", "Search byte"},
    {"beep <freq> <ms>", "PC speaker milliseconds"},
    {"beepus <freq> <us>", "PC speaker microseconds"},
    {"beep_bg <f> <ms>", "PC speaker non-blocking"},
    {"beep_stop", "Stop background beep"},
    {"beep_sequence <f0> <f1> <ms>", "Sweep from f0 to f1 over ms"},
    {"wav_load <name>", "Load 8-bit mono 11025 Hz WAV"},
    {"wav_play", "Play loaded WAV via PC speaker"},
    {"wav_sb16", "Play loaded WAV via Sound Blaster 16"},
    {"wav_stop", "Stop WAV playback"},
    {"playwave <name>", "Load WAV from DB and play via SB16"},
    {"sb16_demo", "Load kasse.wav and play via SB16"},
    {"pcspk <f> <ms>", "Direct PC speaker test"},
    {"pcspk_sweep <f0> <f1> <step> <ms>", "Frequency sweep via PC speaker"},
    {"pcspk_gliss <f0> <f1> <ms>", "Continuous glide without gaps"},
    {"printf \"fmt\"", "Formatted print"},
    {"searchs <addr> <len> <str>", "Search string"},
    {"palflash <idx>", "Cycle one VGA palette entry"},
    {"glyphpulse <code>", "Toggle a glyph bitmap"}
};

// help [cmd]
// - help           => list only command names inline (no formatting)
// - help <command> => show formatted description line for that command
static void extract_base(const char* full, char* out, int outsz){
    if (!full || !out || outsz<=0){ if(out){*out='\0';} return; }
    int i=0; while (full[i] && full[i] != ' ' && full[i] != '<' && full[i] != '"' && i < outsz-1){ out[i]=full[i]; i++; }
    out[i]='\0';
}

void help(const char* name) {
    int count = sizeof(g_help_entries) / sizeof(g_help_entries[0]);
    if (!name || !name[0]) {
        // list only base command names (no usage placeholders)
        char base[64];
        for (int i = 0; i < count; i++) { extract_base(g_help_entries[i].name, base, sizeof(base)); printf("%s ", base); }
        printf("\n");
        return;
    }
    // lookup by base name
    char find[64]; strncpy(find, name, sizeof(find)); find[sizeof(find)-1]='\0';
    for (int i=0;i<count;i++){
        char base[64]; extract_base(g_help_entries[i].name, base, sizeof(base));
        if (strcmp(base, find) == 0){
            // clean format, no tabular padding
            printf("%s - %s\n", g_help_entries[i].name, g_help_entries[i].desc);
            return;
        }
    }
    printf("unknown command: %s\n", name);
}

void searchb(uint32_t address, uint32_t size, uint32_t byte) {
        printf("Searching %d\n", byte);
        void* result = (void*)search_byte((void*)address, size, byte);
        if (result != NULL) {
            char resultAddressStr[20];
            sprintf(resultAddressStr, "%p", &result);
            print_string("Byte found at address: ");
            print_string(resultAddressStr);
            print_string("\n");
        } else {
            print_string("Byte not found.\n");
        }
}

void searchs(uint32_t address, uint32_t size, unsigned char* string) {
        printf("Searching %s\n", &string);
        void* result = (void*)search_string((void*)address, size, string);
        if (result != NULL) {
            char resultAddressStr[20];
            sprintf(resultAddressStr, "%p", &result);
            print_string("String found at address: ");
            print_string(resultAddressStr);
            print_string("\n");
        } else {
            print_string("String not found.\n");
        }
}


void formatTimestamp(unsigned int timestamp_ms) {
    unsigned int ms_per_day = 86400000; // Millisekunden pro Tag
    unsigned int ms_per_hour = 3600000; // Millisekunden pro Stunde
    unsigned int ms_per_minute = 60000; // Millisekunden pro Minute
    unsigned int ms_per_second = 1000;  // Millisekunden pro Sekunde

    unsigned int days = timestamp_ms / ms_per_day;
    timestamp_ms %= ms_per_day;

    unsigned int hours = timestamp_ms / ms_per_hour;
    timestamp_ms %= ms_per_hour;

    unsigned int minutes = timestamp_ms / ms_per_minute;
    timestamp_ms %= ms_per_minute;

    unsigned int seconds = timestamp_ms / ms_per_second;
    unsigned int milliseconds = timestamp_ms % ms_per_second;

    // Ausgabe der Längenzeit
    printf("%d Tage\n", days);
    printf("%d Stunden\n", hours);
    printf("%d Minuten\n", minutes);
    printf("%d Sekunden\n", seconds);
    printf("%d Millisekunden\n", milliseconds);
}

unsigned char strUptime[80];
void formatTimestampHHMMSS(unsigned int timestamp_ms) {
    unsigned int ms_per_day = 86400000; // Millisekunden pro Tag
    unsigned int ms_per_hour = 3600000; // Millisekunden pro Stunde
    unsigned int ms_per_minute = 60000; // Millisekunden pro Minute
    unsigned int ms_per_second = 1000;  // Millisekunden pro Sekunde

    unsigned int days = timestamp_ms / ms_per_day;
    timestamp_ms %= ms_per_day;

    unsigned int hours = timestamp_ms / ms_per_hour;
    timestamp_ms %= ms_per_hour;

    unsigned int minutes = timestamp_ms / ms_per_minute;
    timestamp_ms %= ms_per_minute;

    unsigned int seconds = timestamp_ms / ms_per_second;
    unsigned int milliseconds = timestamp_ms % ms_per_second;

    int revnum = REVISION_NUMBER;
    unsigned char* revdate = REVISION_DATE;
    sprintf(strUptime, "Uptime: %02d:%02d:%02d\n", hours, minutes, seconds);

    print_string(strUptime);
}




void uptime() {
    formatTimestampHHMMSS(GetTicks());
}

void exit() {
    g_bKernelShouldStop = true;
}

void run(void* address) {
    FunctionPointer funcPtr = (FunctionPointer)address;
    funcPtr();
}

void random() {
    for (int i = 0; i < 10; i++) {
        printf("%d\n", rand_range(1, 100)); // Beispiel: Zahlen zwischen 1 und 100
    }
}  

void perftest() {
    if (perf_cpu_hz() == 0) perf_init();
    uint32_t mhz = (uint32_t)(perf_cpu_hz() / 1000000ULL);
    printf("perf: cpu ~ %d MHz\n", mhz);

    uint64_t us0 = micros();
    sleep_us(100000); // 100 ms
    uint64_t us1 = micros();
    uint64_t delta = us1 - us0;
    // print as 32-bit ms/us pairs to avoid 64-bit printf
    uint32_t ms = (uint32_t)(delta / 1000ULL);
    uint32_t us = (uint32_t)(delta - ((uint64_t)ms * 1000ULL));
    printf("micros delta ~ %d ms + %d us\n", ms, us);

    uint64_t us2 = micros();
    sleep(100); // PIT 100 ms
    uint64_t us3 = micros();
    delta = us3 - us2;
    ms = (uint32_t)(delta / 1000ULL);
    us = (uint32_t)(delta - ((uint64_t)ms * 1000ULL));
    printf("PIT sleep(100) measured ~ %d ms + %d us\n", ms, us);
}

void beepus(int freq, int us) { if (us > 0) beep_us(freq, us); }

// WAV wrappers
void wav_load(const char* name){ if (!loadWAV(name)) printf("wav load failed\n"); }
void wav_play_cmd(void){ playWAV(); }
void wav_stop_cmd(void){ stopWAV(); }
void wav_play_sb16_cmd(void){ playWAV_sb16(); }

void playwave_cmd(const char* name){
    if (!name || !name[0]){
        printf("usage: playwave <file.wav>\n");
        return;
    }
    sb16_init();
    if (!sb16_is_present()){
        printf("sb16: device not detected\n");
        return;
    }
    if (!loadWAV(name)){
        printf("playwave: failed to load %s\n", name);
        return;
    }
    playWAV_sb16();
}

void sb16_demo(void){
    sb16_init();
    if (!sb16_is_present()){
        printf("sb16_demo: SB16 not detected\n");
        return;
    }
    if (!loadWAV("kasse.wav")){
        printf("sb16_demo: missing kasse.wav in DB\n");
        return;
    }
    playWAV_sb16();
}

// Print current wall clock time (RTC + micros)
void now(void){ char buf[32]; time_now_iso(buf, sizeof(buf)); printf("%s\n", buf); }


void killtimer() {
    remove_sub_timer(0);
}

int restart() {
    longjmp(&g_jmpKernelMain, 0);
    return 0;
}

void loaddisk() {
    unsigned int total_sectors = 65536;   // 32 MB / 512
    unsigned int chunk_sectors = 128;     // 64KB at a time
    printf("Loading Disk (32MB) to 0x200000\n");
    for(unsigned int i = 0; i < total_sectors; i += chunk_sectors) {
        unsigned int sectors = chunk_sectors;
        if (total_sectors - i < chunk_sectors) {
            sectors = total_sectors - i;
        }
        read_from_disk_fast(i, (void*)(0x200000 + (512 * i)), sectors * 512);
    }
}

void cat(const char *addr) {
    while (*addr != '\0') {
        printf("%c", *addr);
        addr++;
    }
    printf("\n");
}

// ---- DB tools --------------------------------------------------------------
static void _db_ls_cb(const char* name, size_t sz) { printf("%-24s %6d\n", name, (int)sz); }
void db_ls(void){ hbdb_fs_list(_db_ls_cb); }

void db_cat(const char* name){
    unsigned char* data=0; size_t sz=0;
    if (!hbdb_fs_get(name, &data, &sz)) { printf("not found: %s\n", name); return; }
    for (size_t i=0;i<sz;i++) printf("%c", data[i]);
    printf("\n");
    free(data);
}

// Save/load full DB to disk image at 1 MiB (LBA 2048)
static void _print_save_err(int r){
    if (r==0){ printf("db saved\n"); return; }
    if (r==-1) { printf("db save failed: serialize OOM/alloc\n"); return; }
    if (r==-2) { printf("db save failed: payload exceeds max (%d bytes > cap)\n", (int)hbdb_get_max_bytes()); return; }
    if (r==-3) { printf("db save failed: temp buffer alloc\n"); return; }
    printf("db save failed %d\n", r);
}
static void _print_load_err(int r){
    if (r==0){ printf("db loaded\n"); return; }
    if (r==-1){ printf("db load failed: alloc\n"); return; }
    if (r==-3){ printf("db load failed: invalid image or too large\n"); return; }
    if (r==-4){ printf("db load failed: deserialize error\n"); return; }
    printf("db load failed %d\n", r);
}
void db_saveimg(void){ int r = hbdb_save_image(16514); _print_save_err(r); }
void db_loadimg(void){ int r = hbdb_load_image(16514); _print_load_err(r); }

void db_autosave_on(void){ hbdb_set_autosave(1); printf("db autosave: on\n"); }
void db_autosave_off(void){ hbdb_set_autosave(0); printf("db autosave: off\n"); }

void db_info(void){
    int rows = hbdb_fs_rows();
    if (rows < 0) { printf("FS table: missing\n"); return; }
    unsigned int payload = hbdb_calc_bytes_payload();
    unsigned int total   = hbdb_calc_bytes_total();
    unsigned int cap     = hbdb_get_max_bytes();
    unsigned int free    = (total < cap) ? (cap - total) : 0;
    printf("FS rows: %d\n", rows);
    printf("DB size: payload=%d bytes, total(padded)=%d bytes\n", (int)payload, (int)total);
    printf("Capacity: %d bytes (free ~ %d)\n", (int)cap, (int)free);
}

// Wrapper to avoid pointer/int warning in hexdump mapping
void cmd_hexdump(uint32_t addr, uint32_t len){ hexdump((void*)addr, (size_t)len); }

// db_put <addr> <len> <name>
void db_put(uint32_t addr, uint32_t len, const char* name){
    if (!name) { printf("usage: db_put <addr> <len> <name>\n"); return; }
    hbdb_fs_put(name, (const unsigned char*)addr, (size_t)len);
    // commit if autosave is enabled
    hbdb_autosave_maybe();
    printf("put %s (%d bytes)\n", name, (int)len);
}

// db_get <addr> <max> <name>
void db_get(uint32_t addr, uint32_t max, const char* name){
    if (!name) { printf("usage: db_get <addr> <max> <name>\n"); return; }
    unsigned char* data=0; size_t sz=0;
    if (!hbdb_fs_get(name, &data, &sz)) { printf("not found: %s\n", name); return; }
    size_t cp = sz < max ? sz : max;
    memcpy((void*)addr, data, cp);
    free(data);
    printf("get %s -> 0x%p (%d bytes)\n", name, (void*)addr, (int)cp);
}

// db_rm <name>
void db_rm(const char* name){
    if (!name) { printf("usage: db_rm <name>\n"); return; }
    if (hbdb_fs_remove(name)) { hbdb_autosave_maybe(); printf("removed %s\n", name); }
    else printf("not found: %s\n", name);
}

// Editor integration: edit a DB text file
static const char* g_dbedit_name = 0;
static void _editor_save_to_db(unsigned char* buf, int len){ if (!g_dbedit_name) return; hbdb_fs_put(g_dbedit_name, buf, (size_t)len); }
static void _editor_run_from_db(void){ if (!g_dbedit_name) return; if (dobby_launch_thread(g_dbedit_name) >= 0) thread_yield(); }
void db_edit(const char* name){
    g_dbedit_name = name;
    unsigned char* data = 0; size_t sz = 0;
    unsigned char* text_buffer = editor_get_buffer();
    editor_reset_buffer();
    if (hbdb_fs_get(name, &data, &sz) && data) {
        size_t cap = (size_t)editor_buffer_capacity();
        size_t limit = (cap > 0) ? (cap - 1) : 0;
        size_t cp = sz < limit ? sz : limit;
        if (cp > 0) memcpy(text_buffer, data, cp);
        text_buffer[cp] = '\0';
        free(data);
    }
    editor_set_filename_hint(name);
    editor_set_save_callback(_editor_save_to_db);
    editor_set_run_callback(_editor_run_from_db);
    editor_main();
    editor_set_save_callback(0);
    editor_set_run_callback(0);
    g_dbedit_name = 0;
}

// Wrappers for convenient CLI variants
void cmd_memset(uint32_t addr, uint32_t len){ memset((void*)addr, 0, (size_t)len); }
void cmd_memcpy(uint32_t src, uint32_t dst, uint32_t len){ memcpy((void*)dst, (void*)src, (size_t)len); }

void beep_bg(uint32_t freq, uint32_t ms){ beep_start_bg((int)freq,(int)ms); }
void beep_stop(void){ beep_stop_bg(); }

void dtmf_bg(const char* seq){ if (seq && seq[0]) dtmf_start_bg(seq); }
void dtmf_stop(void){ dtmf_stop_bg(); }

int keycodes() {
    while (1) {
        unsigned int sc = getkey();
        // ignore releases (low 8-bit >= 0x80)
        if ((sc & 0xFF) >= 0x80) continue;
        extern const char* scancode_name(unsigned int);
        const char* name = scancode_name(sc);
        if (name && name[0] != '?') printf("%s\n", name);
        if ((sc & 0xFF) == SC_ESC) return 0;
    }
}

// --- TSR clock thread -------------------------------------------------------
static void _tsr_clock_thread(void){
    uint64_t last = (uint64_t)-1;
    while (!thread_should_stop()){
        uint64_t sec = time_now_seconds();
        if (sec != last){
            last = sec;
            char buf[32]; time_now_iso(buf, sizeof(buf));
            int len = (int)strlen(buf);
            int x = 80 - len; if (x < 0) x = 0;
            int cur = get_cursor(); unsigned char old = get_color();
            console_begin_batch();
            set_color(FG_BRIGHT_WHITE | BG_CYAN);
            set_cursor_xy(x, 0);
            printf("%s", buf);
            console_end_batch();
            set_color(old); set_cursor(cur);
        }
        // poll at 10Hz, update exactly when the second flips
        sleep_us(100000);
    }
}

void tsrtime(){ int id = thread_create(_tsr_clock_thread); printf("tsr clock id %d\n", id); if (id < 0) printf("thread create failed\n"); }
void thread_kill_cmd(uint32_t id){ int r = thread_kill((int)id); printf(r==0?"killed %d\n":"kill failed\n", (int)id); }

// Pulse all cells with a given character code by changing FG color between red shades
static void _heart_pulse_thread(void){
    // Fade palette index 4 (RED) between two shades linearly
    const int idx = 4;
    const int steps = 40; const int step_us = 25000; // ~1s per direction
    // endpoints: dark red -> bright red
    int r1=28,g1=0,b1=0, r2=63,g2=0,b2=0;
    while (!thread_should_stop()){
        for (int s=0; s<=steps && !thread_should_stop(); s++){
            int r = r1 + (r2 - r1) * s / steps;
            int g = g1 + (g2 - g1) * s / steps;
            int b = b1 + (b2 - b1) * s / steps;
            vga_set_palette_entry(idx, (uint8_t)r,(uint8_t)g,(uint8_t)b);
            sleep_us(step_us);
        }
        for (int s=0; s<=steps && !thread_should_stop(); s++){
            int r = r2 + (r1 - r2) * s / steps;
            int g = g2 + (g1 - g2) * s / steps;
            int b = b2 + (b1 - b2) * s / steps;
            vga_set_palette_entry(idx, (uint8_t)r,(uint8_t)g,(uint8_t)b);
            sleep_us(step_us);
        }
    }
    // restore to bright red at end
    vga_set_palette_entry(idx, 63,0,0);
}

void heartpulse(){
    // print one visible heart (CP437 0x03) next to id in red
    int id = thread_create(_heart_pulse_thread);
    unsigned char old = get_color(); set_color(FG_RED|BG_BLACK);
    printf("%c ", 3);
    set_color(old);
    printf("heartpulse id %d\n", id);
}

// --- heartspin: rotate heart glyph (0x03) in 90° steps ---------------------
static void rotate90_8xH(const uint8_t* in, uint8_t* out, int H){
    for (int y=0;y<H;y++) out[y]=0;
    int rows = H; int cols = 8;
    for (int y=0;y<rows;y++){
        uint8_t row = in[y];
        for (int x=0;x<cols;x++){
            int bit = (row >> x) & 1;
            if (bit){ int nx = cols-1-y; int ny = x; if (ny < rows){ out[ny] |= (1u<<nx); } }
        }
    }
}

static void _heart_spin_thread(void){
    size_t H = vga_get_font_height(); if (H==0||H>32) H=16;
    uint8_t all[256*32]; save_vga_font(all, H);
    uint8_t orig[32]; for (size_t i=0;i<H;i++) orig[i]=all[3*32 + i];
    uint8_t f0[32], f1[32], f2[32], f3[32];
    for (size_t i=0;i<H;i++) f0[i]=orig[i];
    rotate90_8xH(f0,f1,(int)H);
    rotate90_8xH(f1,f2,(int)H);
    rotate90_8xH(f2,f3,(int)H);
    const uint8_t* frames[4]={f0,f1,f2,f3}; int idx=0;
    while (!thread_should_stop()){
        vga_set_glyph(3, frames[idx], H);
        idx = (idx+1) & 3;
        sleep_us(150000);
    }
    vga_set_glyph(3, orig, H);
}

void heartspin(){ int id = thread_create(_heart_spin_thread); printf("heartspin id %d\n", id); }

// Palfade parameter store keyed by thread id
typedef struct { int set; int idx; int r1,g1,b1; int r2,g2,b2; } PalParam;
static PalParam g_palfade_params[MAX_THREADS];
static int g_pf_c1[3] = {63,63,63};
static int g_pf_c2[3] = {0,42,63};

static void _palfade_thread(void){
    int my = thread_current_id();
    // Wait until parameters have been set by the spawner
    while (!g_palfade_params[my].set && !thread_should_stop()) sleep_us(1000);
    PalParam p = g_palfade_params[my];
    const int steps = 40; const int step_us = 25000; // ~1s per direction
    while (!thread_should_stop()){
        for (int s=0; s<=steps && !thread_should_stop(); s++){
            int r = p.r1 + (p.r2 - p.r1) * s / steps;
            int g = p.g1 + (p.g2 - p.g1) * s / steps;
            int b = p.b1 + (p.b2 - p.b1) * s / steps;
            vga_set_palette_entry(p.idx, (uint8_t)r,(uint8_t)g,(uint8_t)b);
            sleep_us(step_us);
        }
        for (int s=0; s<=steps && !thread_should_stop(); s++){
            int r = p.r2 + (p.r1 - p.r2) * s / steps;
            int g = p.g2 + (p.g1 - p.g2) * s / steps;
            int b = p.b2 + (p.b1 - p.b2) * s / steps;
            vga_set_palette_entry(p.idx, (uint8_t)r,(uint8_t)g,(uint8_t)b);
            sleep_us(step_us);
        }
    }
}

// palflash <idx>
void palflash(uint32_t idx){
    if (idx > 255) idx = 15;
    int id = thread_create(_palfade_thread);
    if (id >= 0){
        g_palfade_params[id].idx = (int)idx;
        g_palfade_params[id].r1 = g_pf_c1[0]; g_palfade_params[id].g1 = g_pf_c1[1]; g_palfade_params[id].b1 = g_pf_c1[2];
        g_palfade_params[id].r2 = g_pf_c2[0]; g_palfade_params[id].g2 = g_pf_c2[1]; g_palfade_params[id].b2 = g_pf_c2[2];
        g_palfade_params[id].set = 1;
    }
    printf("palfade id %d\n", id);
}

// palfade_c1 <r> <g> <b>
void palfade_c1(uint32_t r, uint32_t g, uint32_t b){ g_pf_c1[0]=(int)r; g_pf_c1[1]=(int)g; g_pf_c1[2]=(int)b; }
// palfade_c2 <r> <g> <b>
void palfade_c2(uint32_t r, uint32_t g, uint32_t b){ g_pf_c2[0]=(int)r; g_pf_c2[1]=(int)g; g_pf_c2[2]=(int)b; }

// glyphpulse <code>: invert a glyph every 200ms a few times
void glyphpulse(uint32_t code){
    uint8_t ch = (uint8_t)(code & 0xFF);
    size_t h = vga_get_font_height(); if (h==0||h>32) h=16;
    // Read current glyph into tmp by saving all then slicing
    uint8_t all[256*32];
    save_vga_font(all, h);
    uint8_t orig[32];
    for (size_t i=0;i<h;i++) orig[i] = all[ch*32 + i];

    for (int i=0;i<10;i++){
        // inverted
        uint8_t inv[32]; for (size_t r=0;r<h;r++) inv[r] = (uint8_t)~orig[r];
        vga_set_glyph(ch, inv, h);
        sleep(120);
        // restore
        vga_set_glyph(ch, orig, h);
        sleep(120);
    }
}

// font <name.psf> : load PSF bitmap font (first 256 glyphs)
void font(const char* name){
    if (!name || !name[0]){ printf("usage: font <file.psf>\n"); return; }
    FILE* f = fopen(name, "rb");
    if (!f){ printf("font: not found: %s\n", name); return; }
    unsigned char glyphs[256*32];
    size_t height=0;
    int ok = load_psf_font(f, glyphs, &height);
    fclose(f);
    if (!ok || height==0){ printf("font: unsupported/invalid PSF\n"); return; }
    load_vga_font(glyphs, height);
    printf("font loaded: %s (%d rows)\n", name, (int)height);
}

void kernel_console_clear() {
        clear_screen();
        set_cursor(0);
}

    // message_box and msgbox moved to kernel/ui.c
    
// Diagnostic: raw PC speaker test independent of sub-timers
void pcspk_test(uint32_t freq, uint32_t ms){
    if (ms == 0) ms = 200;
    if (freq < 20) freq = 20; if (freq > 20000) freq = 20000;
    console_play_sound((int)freq);
    console_sound_on();
    sleep_us((uint64_t)ms * 1000ULL);
    console_sound_off();
}

// pcspk_sweep <f0> <f1> <step> <ms_per_step>
void pcspk_sweep(uint32_t f0, uint32_t f1, uint32_t step, uint32_t ms){
    if (step == 0) step = 50;
    if (ms == 0) ms = 50;
    int dir = (f1 >= f0) ? 1 : -1;
    int s = (int)step * dir;
    for (int f = (int)f0; (dir>0)?(f <= (int)f1):(f >= (int)f1); f += s){
        int ff = f; if (ff < 20) ff = 20; if (ff > 20000) ff = 20000;
        console_play_sound((unsigned int)ff);
        console_sound_on();
        sleep_us((uint64_t)ms * 1000ULL);
        console_sound_off();
        sleep_us(10000); // 10 ms gap for clarity
    }
}

void pcspk_sweep3(uint32_t f0, uint32_t f1, uint32_t step){ pcspk_sweep(f0,f1,step,50); }

// pcspk_gliss: continuous frequency glide without gaps using 200us update
void pcspk_gliss(uint32_t f0, uint32_t f1, uint32_t dur_ms){
    if (dur_ms == 0) dur_ms = 500;
    if (f0 < 20) f0 = 20; if (f0 > 20000) f0 = 20000;
    if (f1 < 20) f1 = 20; if (f1 > 20000) f1 = 20000;
    uint64_t us_total = (uint64_t)dur_ms * 1000ULL;
    const uint64_t step_us = 100ULL; // finer updates (0.1 ms)
    uint64_t steps64 = us_total / step_us; if (steps64 < 1) steps64 = 1;
    uint32_t steps = (steps64 > 1000000ULL) ? 1000000u : (uint32_t)steps64;

    // Prime mode once, keep speaker+gate on for entire glide
    port_byte_out(0x43, 0xB6);
    uint8_t v = port_byte_in(0x61) | 0x03; port_byte_out(0x61, v);

    uint64_t t0 = micros();
    for (uint32_t i=0; i<=steps; i++){
        // linear interpolation
        uint32_t freq = (uint32_t)( ( (uint64_t)f0 * (steps - i) + (uint64_t)f1 * i ) / (steps) );
        console_set_freq_nogate((int)freq);
        uint64_t target = t0 + (uint64_t)i * step_us;
        while ((int64_t)(micros() - target) < 0) { /* spin */ }
    }
}
