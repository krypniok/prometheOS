#include "../../include/drivers/display.h"
#include <stdint.h>

// Stubs für fehlende Befehle aus der QEMU-Shell

void help(const char* arg) { printf("Help: %s\n", arg ? arg : "all"); }
void kernel_console_clear(void) { clear_screen(); }
void killtimer(void) { printf("killtimer stub\n"); }

// DB Stubs
void db_ls(void) { printf("db_ls stub\n"); }
void db_cat(const char* s) { printf("db_cat %s stub\n", s); }
void db_edit(const char* s) { printf("db_edit %s stub\n", s); }
void db_saveimg(void) { printf("db_saveimg stub\n"); }
void db_loadimg(void) { printf("db_loadimg stub\n"); }
void db_autosave_on(void) { printf("db_autosave_on stub\n"); }
void db_autosave_off(void) { printf("db_autosave_off stub\n"); }
void db_info(void) { printf("db_info stub\n"); }
void db_put(uint32_t k, uint32_t v, const char* s) { printf("db_put stub\n"); }
void db_get(uint32_t k, uint32_t v, const char* s) { printf("db_get stub\n"); }
void db_rm(const char* s) { printf("db_rm stub\n"); }

// Misc Stubs
void vt_cmd(uint32_t v) { printf("vt %d stub\n", v); }
void font(const char* s) { printf("font %s stub\n", s); }
void restart(void) { printf("restart stub\n"); }
void dobby_cmd(const char* s) { printf("dobby %s stub\n", s); }
void bgademo(void) { printf("bgademo stub\n"); }
void txtmode(void) { printf("txtmode stub\n"); }

// Thread/Sched Stubs
void thread_test(void) { printf("thread_test stub\n"); }
void sched_info(void) { printf("sched_info stub\n"); }
void sched_timeslice(uint32_t v) { printf("timeslice %d stub\n", v); }
void sched_mode(const char* s) { printf("sched_mode %s stub\n", s); }
void thread_kill_cmd(uint32_t v) { printf("kill %d stub\n", v); }

// Audio/Other Stubs
void now(void) { printf("now stub\n"); }
void tsrtime(void) { printf("tsrtime stub\n"); }
void heartpulse(void) { printf("heartpulse stub\n"); }
void heartspin(void) { printf("heartspin stub\n"); }
void printlogo(void) { printf("printlogo stub\n"); }
void uptime(void) { printf("uptime stub\n"); }
void cmd_hexdump(uint32_t a, uint32_t b) { printf("hexdump stub\n"); }
void cmd_memset(uint32_t a, uint32_t b) { printf("memset stub\n"); }
void cmd_memcpy(uint32_t a, uint32_t b, uint32_t c) { printf("memcpy stub\n"); }
void searchb(uint32_t a, uint32_t b, uint32_t c) { printf("searchb stub\n"); }
void beepus(uint32_t a, uint32_t b) { printf("beepus stub\n"); }
void beep_bg(uint32_t a, uint32_t b) { printf("beep_bg stub\n"); }
void beep_stop(void) { printf("beep_stop stub\n"); }
void beep_sequence(uint32_t a, uint32_t b, uint32_t c) { printf("beep_seq stub\n"); }
void pcspk_test(uint32_t a, uint32_t b) { printf("pcspk stub\n"); }
void pcspk_sweep3(uint32_t a, uint32_t b, uint32_t c) { printf("sweep stub\n"); }
void pcspk_gliss(uint32_t a, uint32_t b, uint32_t c) { printf("gliss stub\n"); }
void wav_load(const char* s) { printf("wav_load stub\n"); }
void wav_play_cmd(void) { printf("wav_play stub\n"); }
void wav_play_sb16_cmd(void) { printf("wav_sb16 stub\n"); }
void wav_stop_cmd(void) { printf("wav_stop stub\n"); }
void playwave_cmd(const char* s) { printf("playwave stub\n"); }
void sb16_demo(void) { printf("sb16_demo stub\n"); }
void searchs(uint32_t a, uint32_t b, const char* s) { printf("searchs stub\n"); }
void palflash(uint32_t v) { printf("palflash stub\n"); }
void glyphpulse(uint32_t v) { printf("glyphpulse stub\n"); }
void palfade_c1(uint32_t a, uint32_t b, uint32_t c) { printf("fade1 stub\n"); }
void palfade_c2(uint32_t a, uint32_t b, uint32_t c) { printf("fade2 stub\n"); }
void loaddisk(void) { printf("loaddisk stub\n"); }
void printascii(void) { printf("printascii stub\n"); }
void cat(void* p) { printf("cat stub\n"); }
void run(void* p) { printf("run stub\n"); }
void exit_kernel(void) { printf("exit stub\n"); }
void keycodes(void) { printf("keycodes stub\n"); }
void print_registers(void) { printf("regs stub\n"); }
void random(void) { printf("random stub\n"); }
void perftest(void) { printf("perftest stub\n"); }
void dtmf_bg(const char* s) { printf("dtmf_bg stub\n"); }
void dtmf_stop(void) { printf("dtmf_stop stub\n"); }

// Taskmgr stubs
int thread_enumerate(void* infos, int max) { return 0; }
int thread_current_id(void) { return 0; }
int thread_kill(int tid) { return -1; }
void thread_init(void) {}
int thread_create(void (*fn)(void)) { return -1; }
void thread_exit(void) {}
void thread_yield(void) {}
int thread_should_stop(void) { return 0; }
void thread_preempt_tick(registers_t* regs) {}