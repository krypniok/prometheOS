# --- Konfiguration ---
# Verzeichnisse
BOOT_DIR = boot
KERNEL_DIR = kernel
DRIVERS_DIR = drivers
LIB_DIR = lib
BUILD_DIR = build
INCLUDE_DIR = include

CC = gcc
LD = ld
OBJCOPY = objcopy

# Flags für 16-bit GCC-Code (MBR-Stil)
CFLAGS = -Os -fomit-frame-pointer -m32 -fno-pic -ffreestanding -nostdlib -fno-stack-protector -m16 -march=i386 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions
# Flags für echten 32-bit Kernel Code (ohne -m16)
# NEU: -I$(INCLUDE_DIR) damit Header in include/ gefunden werden
CFLAGS_32 = -Os -fomit-frame-pointer -m32 -fno-pic -ffreestanding -nostdlib -fno-stack-protector -march=i386 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -I$(INCLUDE_DIR)

LDFLAGS = -m elf_i386 -T $(BOOT_DIR)/linker.ld --oformat binary
LDFLAGS_PRELOADER = -m elf_i386 -T $(BOOT_DIR)/preloader.ld --oformat binary
LDFLAGS_KERNEL = -m elf_i386 -T $(KERNEL_DIR)/kernel.ld --oformat binary

TARGET = $(BUILD_DIR)/prometheos.img
VM_HDD = prometheos.vdi
BOOT_BIN = $(BUILD_DIR)/boot.bin
PRELOADER_BIN = $(BUILD_DIR)/preloader.bin
VM_NAME = PrometheOS_Vault
LOG_FILE = serial_debug.log

# --- Automatische Quellcode-Suche ---
# Wir suchen in kernel, drivers, lib und fs (falls vorhanden) nach .c Dateien
SRC_DIRS = $(KERNEL_DIR) $(DRIVERS_DIR) $(LIB_DIR) fs

# Findet alle .c Dateien rekursiv (2>/dev/null unterdrückt Fehler bei fehlenden Ordnern)
KERNEL_SOURCES = $(shell find $(SRC_DIRS) -name '*.c' 2>/dev/null)
# Erstellt eine Liste der Objektdateien im Build-Ordner (Struktur bleibt erhalten)
KERNEL_OBJECTS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(KERNEL_SOURCES))

# --- Targets ---

all: $(TARGET)

# Den MBR aus der boot.c bauen
$(BOOT_BIN): $(BOOT_DIR)/boot.c $(BOOT_DIR)/linker.ld | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $(BUILD_DIR)/boot.o
	$(LD) $(LDFLAGS) $(BUILD_DIR)/boot.o -o $@

# Den Preloader bauen
$(PRELOADER_BIN): $(BOOT_DIR)/preloader.c $(BOOT_DIR)/preloader.ld | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $(BUILD_DIR)/preloader.o
	$(LD) $(LDFLAGS_PRELOADER) $(BUILD_DIR)/preloader.o -o $@

# Den Kernel bauen (Kernel + Treiber)
# Hängt jetzt von der automatischen Objekt-Liste ab
$(BUILD_DIR)/kernel.bin: $(KERNEL_OBJECTS) $(KERNEL_DIR)/kernel.ld
	$(LD) $(LDFLAGS_KERNEL) $(KERNEL_OBJECTS) -o $@

# Generische Regel für alle C-Dateien in den Quellordnern
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_32) -c $< -o $@

# Das 64MB Disk Image erstellen (HDD)
$(TARGET): $(BOOT_BIN) $(PRELOADER_BIN) $(BUILD_DIR)/kernel.bin | $(BUILD_DIR)
	@echo "[+] Erstelle Disk Image (64MB)..."
	dd if=/dev/zero of=$(TARGET) bs=1M count=64
	dd if=$(BOOT_BIN) of=$(TARGET) bs=512 count=1 conv=notrunc
	
	# Partitionstabelle erstellen (Partition 1 ab Sektor 2048, Typ 0x0C FAT32 LBA)
	# Wir nutzen sfdisk, um eine saubere Tabelle im MBR zu erzeugen.
	echo "2048,,0c" | sfdisk $(TARGET)
	
	# Partition formatieren (FAT32)
	# Wir erstellen ein temporäres Image für die Partition, formatieren es und fügen es ein.
	dd if=/dev/zero of=$(BUILD_DIR)/partition.img bs=1M count=60
	mkfs.vfat -F 32 $(BUILD_DIR)/partition.img
	dd if=$(BUILD_DIR)/partition.img of=$(TARGET) bs=512 seek=2048 conv=notrunc
	rm $(BUILD_DIR)/partition.img
	
	# Preloader & Kernel in die "Hidden Sectors" (zwischen MBR und Partition 1) schreiben
	dd if=$(PRELOADER_BIN) of=$(TARGET) bs=512 seek=1 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$(TARGET) bs=512 seek=20 conv=notrunc

# Konvertiere RAW Image zu VDI für VirtualBox
$(VM_HDD): $(TARGET)
	# Sicherstellen, dass die VM die Disk nicht blockiert (Detach)
	vboxmanage controlvm "$(VM_NAME)" poweroff 2>/dev/null || true
	vboxmanage storageattach "$(VM_NAME)" --storagectl "IDE" --port 0 --device 0 --type hdd --medium none 2>/dev/null || true
	# Entferne altes Medium aus VBox Registry, um UUID-Konflikte zu vermeiden
	vboxmanage closemedium disk $(shell pwd)/$(VM_HDD) --delete 2>/dev/null || true
	rm -f $(VM_HDD)
	vboxmanage convertfromraw $(TARGET) $(VM_HDD) --format VDI

# VirtualBox VM Setup & Start
# Erstellt die VM, falls sie nicht existiert, und startet sie
run: $(VM_HDD)
	@echo "[+] Starte VirtualBox..."
	# VM stoppen, falls sie noch läuft
	vboxmanage controlvm "$(VM_NAME)" poweroff 2>/dev/null || true
	
	# Prüfen, ob VM existiert, sonst erstellen (idempotent)
	vboxmanage list vms | grep -q "$(VM_NAME)" || \
		vboxmanage createvm --name "$(VM_NAME)" --ostype Other --register

	# Apply/ensure VM settings (safe to run repeatedly)
	# Basis-Einstellungen (Memory, VRAM, UART, Boot)
	vboxmanage modifyvm "$(VM_NAME)" --memory 64 --vram 32 --uart1 0x3F8 4 --uartmode1 file $(shell pwd)/$(LOG_FILE) --boot1 disk --boot2 none || true
	
	# Audio-Einstellungen (PulseAudio + SB16) - Split command for VBox 6/7 compatibility
	# Wir lassen den Treiber weg (Auto-Detect), das ist oft stabiler als 'pulse' zu erzwingen.
	vboxmanage modifyvm "$(VM_NAME)" --audio-controller sb16 --audio-enabled on --audio-out on 2>/dev/null || vboxmanage modifyvm "$(VM_NAME)" --audio-controller sb16 --audio-enabled on || true

	# Ensure IDE controller exists
	vboxmanage storagectl "$(VM_NAME)" --name "IDE" --add ide 2>/dev/null || true

	# Attach Disk Image as IDE HDD
	vboxmanage storageattach "$(VM_NAME)" --storagectl "IDE" --port 0 --device 0 --type hdd --medium none 2>/dev/null || true
	vboxmanage storageattach "$(VM_NAME)" --storagectl "IDE" --port 0 --device 0 --type hdd --medium $(shell pwd)/$(VM_HDD)

	# VM starten (Headless oder GUI - hier GUI für dich)
	virtualboxvm --startvm "$(VM_NAME)" &

# Log-Datei live mitverfolgen (wie tail -f)
log:
	@touch $(LOG_FILE)
	tail -f $(LOG_FILE)

# Build-Verzeichnis erstellen
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	# Aufräumen: VM stoppen und Medium freigeben
	vboxmanage controlvm "$(VM_NAME)" poweroff 2>/dev/null || true
	vboxmanage closemedium disk $(shell pwd)/$(VM_HDD) --delete 2>/dev/null || true
	rm -rf $(BUILD_DIR) $(VM_HDD) $(LOG_FILE)
	@echo "[!] Alles gesäubert."

.PHONY: all run clean log