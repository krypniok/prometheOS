// fpu.c

// einmalig beim start aufrufen
void fpu_init(void) {
    __asm__ __volatile__("fninit");
}

// argument reduction: x = x mod 2π
double fpu_reduce(double x) {
    double r;
    __asm__ __volatile__ (
        "fldl %1;"        // x
        "fldpi;"          // π
        "fadd %%st, %%st;"// 2π
        "fprem;"          // x mod 2π
        "fstpl %0;"       // -> r
        : "=m"(r) : "m"(x)
    );
    return r;
}

double fpu_sin(double x) {
    double r;
    x = fpu_reduce(x);
    __asm__ __volatile__ (
        "fldl %1;"
        "fsin;"
        "fstpl %0;"
        : "=m"(r) : "m"(x)
    );
    return r;
}

double fpu_cos(double x) {
    double r;
    x = fpu_reduce(x);
    __asm__ __volatile__ (
        "fldl %1;"
        "fcos;"
        "fstpl %0;"
        : "=m"(r) : "m"(x)
    );
    return r;
}
