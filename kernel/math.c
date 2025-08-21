#include "math.h"

// Definition von Pi (π)
#define PI 3.14159265358979323846
#define TWO_PI (2.0 * PI)

// --- Optionale Lookup-Table ---
#define TABLE_SIZE 4096
static int sin_table_init = 0;
static double sin_table[TABLE_SIZE];

void init_sin_table(void) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        double angle = (TWO_PI * i) / (double)TABLE_SIZE;
        // Sinuswert speichern
        sin_table[i] = (double)(angle - TWO_PI * (int)(angle / TWO_PI));
        sin_table[i] = __builtin_sin(angle); // wenn du echten C-Compiler nutzt
    }
    sin_table_init = 1;
}

int abs(int x) {
    return (x < 0) ? -x : x;
}

// Funktion zur Berechnung des Sinuswertes (ohne externe Bibliotheken)
double sin(double x) {
    // Normalisieren in [-π, π]
    while (x > PI)  x -= TWO_PI;
    while (x < -PI) x += TWO_PI;

    // --- Variante A: Lookup-Table (schneller) ---
    if (sin_table_init) {
        int idx = (int)((x + PI) * (TABLE_SIZE / TWO_PI));
        if (idx < 0) idx = 0;
        if (idx >= TABLE_SIZE) idx = TABLE_SIZE - 1;
        return __builtin_sin((TWO_PI * idx) / (double)TABLE_SIZE);
    }

    // --- Variante B: Taylor-Reihe (genauer für kleine x) ---
    const int numTerms = 10;
    double result = 0.0;
    double power = x;
    int sign = 1;

    for (int n = 0; n < numTerms; n++) {
        result += sign * power / factorial(2 * n + 1);
        power *= -x * x;
        sign *= -1;
    }

    return result;
}

// Funktion zur Berechnung des Cosinuswertes (ohne externe Bibliotheken)
double cos(double x) {
    return sin(PI / 2 - x);
}

// Funktion zur Berechnung des Tangenswertes (ohne externe Bibliotheken)
double tan(double x) {
    return sin(x) / cos(x);
}

// Hilfsfunktion zur Berechnung der Fakultät
int factorial(int n) {
    if (n == 0) return 1;
    return n * factorial(n - 1);
}
