#!/bin/bash
echo "Bereinige doppelte Dateien..."

# Lösche alte Dateien, die durch den Import ersetzt wurden
rm -f kernel/memory.c
rm -f kernel/drivers/timer.c
rm -f lib/string.c
rm -f drivers/sb16/sb16.c
rm -f drivers/timer.c
rm -f kernel/interrupts.c
rm -f kernel/scheduler.c

echo "Bereinigung abgeschlossen."
chmod +x fix_conflicts.sh