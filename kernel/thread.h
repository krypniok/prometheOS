/* thread.h */
#ifndef THREAD_H
#define THREAD_H

#include "../cpu/jmpbuf.h"
#include "../cpu/isr.h"

#define MAX_THREADS  16
#define STACK_SIZE   4096

// Thread‑Control‑Block (TCB)
typedef struct thread {
    jmp_buf ctx;               // Kontext für kooperatives Umschalten
    unsigned char *stack;      // Basisadresse des Kernel‑Stacks
    int active;                // 1 = lauffähig, 0 = frei
    void (*entry)(void);       // Entry‑Funktion (präemptiv)
    registers_t* frame;        // Gesicherter IRQ‑Frame (präemptiv)
    volatile int should_stop;  // Signal für kooperatives Beenden
} thread_t;

typedef struct thread_info {
    int id;
    int active;
    int should_stop;
    void (*entry)(void);
    registers_t* frame;
    unsigned char *stack;
} thread_info_t;

void thread_init(void);               // Scheduler initialisieren
int  thread_create(void (*fn)(void)); // neuen Thread starten
void thread_yield(void);              // kooperativer Kontextwechsel
void thread_exit(void);               // aktuellen Thread beenden
int  thread_join(int thread_id);      // auf Threadende warten (optional)
int  thread_kill(int thread_id);      // Thread zum Stoppen signalisieren
int  thread_should_stop(void);        // Abbruchsignal des eigenen Threads
int  thread_current_id(void);         // aktuelle Thread‑ID
int  thread_enumerate(thread_info_t* out, int max_entries);
int  thread_count(void);

// Simple demo/test command: spawns two threads and schedules them
void thread_test(void);

// Scheduler control/inspection
void sched_info(void);                     // Infos zu Timeslice/Modus
void sched_timeslice(unsigned int ms);     // Timeslice in ms setzen
void sched_mode(const char* mode);         // "preempt" oder "coop"

// Preemptive scheduler hook (IRQ0)
void thread_preempt_tick(registers_t* r);  // IRQ0‑Hook für Präemption

#endif
