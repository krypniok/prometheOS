# PrometheOS64

PrometheOS64 ist ein eigenständiges 32-Bit Protected Mode Betriebssystem, das vollständig in C (mit minimalem Inline-Assembly) entwickelt wurde. Es demonstriert Low-Level-Programmierung, Hardware-Treiber-Implementierung und Grafik-Rendering ohne externe Bibliotheken.

## Projektstruktur

- **/boot**: Enthält den Bootloader (`boot.c`, MBR in C) und den Preloader (`preloader.c`), die das System in den Protected Mode schalten.
- **/kernel**: Enthält den Hauptkernel (`kernel.c`), der die Hardware steuert und die Shell bereitstellt.
- **Makefile**: Build-Skript zum Kompilieren und Erstellen des Disk-Images.

## Funktionalitäten

Der Kernel bietet folgende Features, die "from scratch" implementiert wurden:

### 🖥️ Grafik & UI
- **VBE Grafikmodus**: Unterstützung für High-Color/True-Color Grafikmodi (via Linear Framebuffer).
- **Software Rendering**: Eigene Implementierung von Zeichenfunktionen:
  - `put_pixel`: Pixelsetzen mit Pitch- und BPP-Korrektur.
  - `draw_rect`, `draw_circle`: Primitive Formen.
  - `draw_char`, `draw_number`: Textausgabe unter Verwendung eines BIOS-Fonts.
- **Text-Konsole**: Emulation eines Textmodus auf dem Grafikbildschirm mit Scrolling und Cursor.

### ⌨️ Eingabe
- **PS/2 Tastatur**: Interrupt-gesteuerter Treiber mit Scancode-Mapping und Pufferung.
- **PS/2 Maus**: Initialisierung und Interrupt-Handling (IRQ 12), Bewegung des Mauszeigers auf dem Bildschirm.

### 🔊 Audio (Sound Blaster 16)
- **Treiber**: Vollständiger SB16 Treiber mit DSP-Reset und Mixer-Konfiguration.
- **Synthese**: Echtzeit-Sinus-Synthese über die FPU (`fsin`) für Beeps und Töne.
- **DMA**: Nutzung des ISA DMA Controllers (Channel 1) für Audio-Streaming.
- **Features**: Stereo-Wiedergabe, Frequenz-Sweeps und binaurale Beats.

### ⏱️ Multitasking & System
- **Scheduler**: Preemptives Multitasking mit Round-Robin Scheduling.
- **Timer**: Programmierung des PIT (Programmable Interval Timer) auf 1000 Hz.
- **Interrupts**: Eigene IDT (Interrupt Descriptor Table) und ISRs (Interrupt Service Routines) für Timer, Tastatur und Maus.

### 💾 Speicher & Disk
- **ATA Treiber**: PIO-Mode Treiber zum Lesen von Festplattensektoren (z.B. MBR Partitionstabelle).
- **Speicher**: Erkennung der RAM-Größe und VBE-Video-Speicher.

## Shell Befehle

Das System bootet in eine interaktive Shell (`simon@prometheos:/>`). Verfügbare Befehle:

- `help`: Zeigt die Hilfe an.
- `cpuid`: Zeigt CPU-Hersteller und Brand-String an.
- `memsize`: Zeigt den verfügbaren System-RAM.
- `vbe`: Zeigt Informationen zum VBE-Controller und Videospeicher.
- `gfx`: Zeichnet ein Testbild mit Farbverläufen und Formen.
- `pong`: Startet das Spiel "Pong" (Maussteuerung vs. KI).
- `beep <hz> <ms>`: Erzeugt einen Ton (z.B. `beep 440 500`).
- `sb16 <l> <r> <ms>`: Spielt binaurale Töne (Stereo).
- `disk`: Liest und zeigt die Partitionstabelle (MBR) der Festplatte.
- `music`: Startet einen Hintergrund-Task, der einen Beat spielt.
- `sleep`: Testet die Sleep-Funktion.
- `clear`: Löscht den Bildschirm.

## Build & Run

```bash
make run
```