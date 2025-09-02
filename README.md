# PrometheOS 🔥

## Beschreibung
PrometheOS ist ein selbstgeschmiedetes 32-bit x86 Betriebssystem,  
gebaut von Grund auf – Bootsektor, Kernel, Treiber, alles Handarbeit.  

Wie sein Namensgeber **Prometheus** bringt es das Feuer (Technik, Wissen)  
in die kalte Leere der Maschine.  
Kein Mainstream-OS, sondern ein Lern- und Forschungsprojekt:  
Assembler trifft C, QEMU/Bochs werden zum Labor,  
und jedes Bit wird von Hand gesetzt.  

## Ziele
- Minimaler, verständlicher Kernel  
- Grafikmodus (BGA), Tastatur & Maus  
- Multitasking & eigene Systemaufrufe  
- Spielwiese für Experimente jenseits der Lehrbücher  

## Status
- Bootloader läuft
- Protected Mode aktiviert
- Erste Kernelroutinen implementiert
- Grafik & Input in Arbeit

## Funktionalitäten
- Bootloader & Protected Mode, CPU/IDT, Timer/Sub-Timer
- VGA- und BGA-Grafik, Tastatur, Maus, HDD, DMA, Debug
- Kernel-Konsole, Speicherverwaltung, FPU, Random, Perf-Counter, DTMF/Beep
- Programme (Editor, Hexviewer, Snake, SB16-Demo)
- Bibliotheken (string/stdio/memory, Text-UI)
- HomebrewDB: einfache, integrierte DB als Dateisystem-Ersatz
  - Dateien sind Blobs in der Tabelle `FS(name STRING, data BLOB)`
  - Standard-API: `fopen/fread/fwrite/fclose` via `stdlibs/stdio_fs.h`
  - Persistenz ins Disk-Image ab 1 MiB (LBA 2048)
  - Kernel lädt DB beim Boot; Autosave optional/standardmäßig an

### HomebrewDB (HBDB)
- Max. DB-Payload im Kernel: 8 MiB (konfigurierbar)
- Host-Tool `hbdbtool` (ls/cat/put/get/rm) für `database.hbdb`
- Makefile injiziert nur noch `database.hbdb` (kein separates `logo.bmp`)

#### Kernel-Kommandos (Auszug)
- `db_ls`, `db_cat <name>`: Dateien anzeigen
- `db_edit <name>`: Datei im Editor öffnen (F2 speichert, F5 führt `dobby <name>` aus)
- `db_put <addr> <len> <name>`, `db_get <addr> <max> <name>`, `db_rm <name>`
- `db_loadimg`, `db_saveimg`: DB <-> Image (schreibt nur benötigte Länge)
- `db_autosave_on/off`: Speichern bei `fclose` aktivieren/deaktivieren
- `dobby <name>`: Little C Script aus DB ausführen

#### Editor
- F2: speichert in die DB (nicht auf Disk direkt)
- F5: führt `dobby` für die bearbeitete DB-Datei aus

#### Image-Layout & Makefile
- Layout (LBA / Sektoren à 512 Byte):
  - 0: MBR (512 B)
  - 1: frei (reservert)
  - 2..127: kernel_stub.bin
  - 128..129: Puffer
  - 130..(130+16383): Kernel (max. 8 MiB, 16384 Sektoren)
  - 16514..(16514+65535): HomebrewDB (32 MiB)
- `make`: baut Kernel/Image, startet QEMU nicht
- `make run`: baut und startet QEMU, injiziert `database.hbdb` ab LBA 16514, extrahiert 32 MiB zurück

### Migration auf HBDB
- Alte TinySQL-Befehle entfernt (tinysql/tinysql2). HBDB ersetzt Speicherung vollständig.

---

„PrometheOS – mehr als ein OS, ein Funke.“  
