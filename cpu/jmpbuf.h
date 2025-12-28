#ifndef JMPBUF_H
#define JMPBUF_H

typedef struct {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int esi;
    unsigned int edi;
    unsigned int ebp;
    unsigned int esp;
    unsigned int eip;
} jmp_buf;

extern int setjmp(jmp_buf *env);
extern void longjmp(jmp_buf *env, int val);

#endif
