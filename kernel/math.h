

// Definition von Pi (π)
#define PI 3.14159265358979323846
// Makro zur Konvertierung der Frequenz in Hertz in die erforderlichen LSB und MSB Werte
#define FREQUENCY_TO_LSB_MSB(frequency) \
    ((uint16_t)(1193180 / (frequency)))


// Definition von Pi (π)
#define PI 3.14159265358979323846

int abs(int x);

// Funktion zur Berechnung des Sinuswertes (ohne externe Bibliotheken)
double sin(double x);
// Funktion zur Berechnung des Cosinuswertes (ohne externe Bibliotheken)
double cos(double x);
// Funktion zur Berechnung des Tangenswertes (ohne externe Bibliotheken)
double tan(double x);
// Hilfsfunktion zur Berechnung der Fakultät
int factorial(int n);

void init_sin_table(void);