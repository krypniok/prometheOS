# PrometheOS 🔥

Kleines 32‑Bit x86‑Kernel‑Playground für QEMU: VGA‑Text, BGA‑Grafik, Keyboard/Mouse, SB16‑Audio, Threads (kooperativ/präemptiv), Zeitmessung via TSC – plus „Dobby“, ein eingebetteter Little‑C‑Interpreter mit Minidatenbank.

## Quick Start

Voraussetzungen: `qemu-system-i386` (oder x86_64), `nasm`, `gcc`, `make`.

- `make`        – baut Kernel + Image
- `make run`    – startet QEMU (HDD inkl. DB)
- `make debug`  – startet QEMU mit GDB‑Stub (Port 1234)

### Payload / Database (HomebrewDB)

Dateien für Dobby und Assets liegen in `payload/` und werden zu `database.hbdb` gebündelt.

- `make payload-init` – legt `payload/` an und verschiebt Beispiele (bmp, *.c)
- `make database`     – baut die DB aus allen Dateien in `payload/`
- `make database-ls`  – listet DB‑Inhalt

Beispielablauf:

```
make payload-init
make database
make run
```

Im Kernel‑Prompt: `db_ls`, `dobby beep.c`, `dobby bgatest.c`, `dobby bga_gradient.c`.

## Kernel‑Befehle (Auswahl)

- `help`, `help <cmd>`
- `demo` (BGA), `sb16` (Soundblaster)
- `dobby <name>` (Little C aus DB)
- `thread_test`, `sched_info`, `sched_mode <preempt|coop>`
- `db_ls`, `db_cat <name>`, `db_edit <name>`

## BGA‑Grafik

API in `kernel/bga_video.h` (deutsche Kurz‑Doku im Header):
- `bga_init(w,h)` / `bga_close()`
- `bga_clear(c)`, `bga_drawpixel(x,y,c)`
- `bga_drawline(...)`, `bga_drawtri(...)`
- `bga_blit(...)`, `bga_is_active()`, `bga_width()`, `bga_height()`

Dobby‑Wrapper: identische Funktionsnamen verfügbar. Beispiele:
- `payload/bgatest.c` – Linien, Fadenkreuz, Dreieck
- `payload/bga_gradient.c` – Pixel‑Gradient

Hinweis Little‑C:
- Nur `/* ... */`‑Kommentare (kein `//`).
- Getypte Funktionsköpfe (`int foo(...)`) werden unterstützt.

## Threads / Zeit / Audio

- Threads: `kernel/thread.h` (Kurz‑Doku im Header). Kooperatives `thread_yield()`, Präemption per IRQ0.
- Zeit/Perf: zentrales `kernel/time.h` (sleep, sleep_us, micros, millis). RTC in `kernel/rtc.h`.
- Audio (PC‑Speaker): `beep`, `beep_sequence`, DTMF‑Helfer (siehe `kernel/conio.h`).

## Struktur

- `kernel/` – Kernelsubsyst. (Grafik, Konsole, Zeit, UI, Scheduler)
- `drivers/` – VGA/Ports/PCI/Keyboard/Mouse/SB16
- `cpu/` – IDT/ISR/Timer/jmpbuf
- `stdlibs/` – kleine libc‑Helfer, DB, BMP, Font
- `programs/` – Demos/Editor; `programs/dobby/` – Interpreter
- `payload/` – Dateien, die in die DB gebündelt werden

## Troubleshooting

- „semicolon expected“ in Dobby: `;` vergessen oder falsche Kommentarart.
- Kein Ton: Audio‑Backend von QEMU prüfen (`make test-env`).

—

„PrometheOS – mehr als ein OS, ein Funke.“
