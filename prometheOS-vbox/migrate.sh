#!/bin/bash
# migrate.sh - Massen-Import von prometheOS-qemu nach prometheOS-vbox

SRC="../prometheOS-qemu"
DST="."

if [ ! -d "$SRC" ]; then
    echo "FEHLER: Quellverzeichnis $SRC nicht gefunden!"
    exit 1
fi

echo "[*] Starte Migration von $SRC nach $DST..."

# 1. Verzeichnisstruktur in VBox erweitern
echo "[*] Erstelle Ziel-Verzeichnisse..."
mkdir -p $DST/kernel/drivers
mkdir -p $DST/kernel/lib
mkdir -p $DST/kernel/programs
mkdir -p $DST/kernel/fs
mkdir -p $DST/kernel/cpu
mkdir -p $DST/include/drivers
mkdir -p $DST/include/lib
mkdir -p $DST/include/programs
mkdir -p $DST/include/fs
mkdir -p $DST/include/cpu
mkdir -p $DST/include/net

# 2. Funktion zum Kopieren von Modulen (Source -> Kernel, Header -> Include)
function copy_module {
    local src_sub=$1    # Unterordner in QEMU (z.B. drivers)
    local dst_src=$2    # Ziel für .c (z.B. kernel/drivers)
    local dst_inc=$3    # Ziel für .h (z.B. include/drivers)
    
    if [ -d "$SRC/$src_sub" ]; then
        echo "    Importiere: $src_sub -> $dst_src"
        cp $SRC/$src_sub/*.c $DST/$dst_src/ 2>/dev/null
        cp $SRC/$src_sub/*.h $DST/$dst_inc/ 2>/dev/null
    else
        echo "    WARNUNG: $src_sub nicht in Quelle gefunden."
    fi
}

# 3. Massen-Kopieren
copy_module "drivers"   "kernel/drivers"  "include/drivers"
copy_module "stdlibs"   "kernel/lib"      "include/lib"
copy_module "programs"  "kernel/programs" "include/programs"
copy_module "fs"        "kernel/fs"       "include/fs"
copy_module "cpu"       "kernel/cpu"      "include/cpu"

# Kernel-Utils (BGA, UI, etc.) die in QEMU direkt im kernel/ Ordner lagen
echo "    Importiere Kernel-Utils (UI, BGA)..."
cp $SRC/kernel/bga_video.c $DST/kernel/ 2>/dev/null
cp $SRC/kernel/ui.c        $DST/kernel/ 2>/dev/null
cp $SRC/kernel/time.c      $DST/kernel/ 2>/dev/null
# Header aus kernel/
cp $SRC/kernel/*.h         $DST/include/ 2>/dev/null

echo "[*] Migration abgeschlossen."
echo "WICHTIG: Du musst dein Makefile anpassen, um die neuen .c Dateien in kernel/drivers, kernel/lib etc. mitzukompilieren!"
chmod +x migrate.sh