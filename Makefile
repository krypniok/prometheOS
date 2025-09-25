# $@ = target file
# $< = first dependency
# $^ = all dependencies

.PHONY: all run rerun echo debug clean test-env
# Database tooling
.PHONY: database database-ls database-clean payload-init

# --- Quellen ---
C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c stdlibs/*.c programs/*.c programs/dobby/*.c)
HEADERS   = $(wildcard kernel/*.h  drivers/*.h cpu/*.h stdlibs/*.h programs/*.h programs/dobby/*.h)

# --- Default ---
# Build artifacts only; do not start QEMU unless 'make run'
all: os-image.bin

# --- Revision bump (wie gehabt) ---
CURRENT_REVISION := $(shell cat version.txt)
CURRENT_DATE     := $(shell date)
NEW_REVISION     := $(shell expr $(CURRENT_REVISION) + 1)
$(shell echo $(NEW_REVISION) > version.txt)

# Add -DENABLE_DEBUG when building with DEBUG=1
CFLAGS := -DREVISION_NUMBER=$(NEW_REVISION) -DREVISION_DATE='"$(CURRENT_DATE)"' $(if $(DEBUG),-DENABLE_DEBUG,)

# --- Interpreter (LittleC) ---
LITTLEC_OBJS = programs/dobby/parser.o \
                programs/dobby/littlec.o \
                programs/dobby/lclib.o

# --- Objektlisten ---
COMMON_OBJS = boot/kernel_entry.o \
              kernel/bga_minimal.o \
              kernel/bga_video.o \
              cpu/interrupt.o cpu/setjmp.o cpu/longjmp.o \
              kernel/conio.o kernel/kernel_command.o kernel/math.o kernel/fpu.o kernel/ui.o \
              kernel/mem.o kernel/time.o kernel/util.o kernel/perf.o kernel/rtc.o \
              drivers/debug.o drivers/display.o drivers/hdd.o drivers/hidden_cmd.o \
              drivers/keyboard.o drivers/mouse.o drivers/ports.o drivers/video.o drivers/dma.o drivers/pci.o \
              cpu/idt.o cpu/isr.o cpu/timer.o \
              cpu/cpuinfo.o \
              stdlibs/file.o stdlibs/memory.o stdlibs/stdio.o stdlibs/string.o \
              stdlibs/homebrewdb.o \
              stdlibs/bmp.o stdlibs/font_psf.o \
              stdlibs/tsqlfs.o \
              programs/editor.o kernel/logo.o \
              programs/snake.o programs/snaketext.o \
              kernel/thread.o \
              $(LITTLEC_OBJS)

COMMON_OBJS_STUB = boot/kernel_entry.o \
              cpu/interrupt.o cpu/setjmp.o cpu/longjmp.o \
              kernel/conio.o kernel/math.o kernel/perf.o kernel/thread.o \
              kernel/mem.o kernel/time.o kernel/util.o kernel/rtc.o \
              drivers/debug.o drivers/display.o drivers/hdd.o drivers/hidden_cmd.o \
              drivers/keyboard.o drivers/ports.o drivers/video.o drivers/pci.o \
              cpu/idt.o cpu/isr.o cpu/timer.o \
              stdlibs/file.o stdlibs/memory.o stdlibs/stdio.o stdlibs/string.o \
              stdlibs/tsqlfs.o stdlibs/homebrewdb.o

# --- Build Kernel Stub + Kernel ---
kernel_stub.bin: $(COMMON_OBJS_STUB) kernel/kernel_stub.o
	ld -nostdlib -m elf_i386 -o $@ -Ttext 0x10000 $^ --oformat binary

kernel.bin: $(COMMON_OBJS) kernel/kernel.o linker.ld
	ld -nostdlib -m elf_i386 -T linker.ld -o $@ $(filter-out linker.ld,$^) --oformat binary
	@# Enforce max kernel size 8 MiB
	@test $$(stat -c%s kernel.bin) -le 8388608 || (echo "Kernel exceeds 8 MiB" && false)

# --- Image bauen mit festen Offsets ---
os-image.bin: boot/mbr.bin kernel_stub.bin kernel.bin
	# 42MB-Image vorbereiten (genug Platz fuer Kernel + 32MiB DB)
	# Erforderlich: bis LBA 82049 -> ~40.1MiB, wir runden auf 42MiB (86016 Sektoren)
	dd if=/dev/zero of=$@ bs=512 count=86016
	# MBR nach Sektor 0
	dd if=boot/mbr.bin of=$@ conv=notrunc bs=512 seek=0
	# Einen Sektor frei lassen (Sektor 1)
	# Stub ab Sektor 2
	dd if=kernel_stub.bin of=$@ conv=notrunc bs=512 seek=2
	# 2 Sektoren Puffer (128–129) bleiben leer (historisch)
	# Großer Kernel ab Sektor 130 (LBA_KERNEL=130). Maximal 8MiB (16384 Sektoren)
	dd if=kernel.bin of=$@ conv=notrunc bs=512 seek=130

# --- QEMU detect + CPU (32-bit Kernel auf 64-bit QEMU erzwingen) ---
QEMU32 := $(shell command -v qemu-system-i386 2>/dev/null)
QEMU64 := $(shell command -v qemu-system-x86_64 2>/dev/null)
QEMU   := $(if $(QEMU32),$(QEMU32),$(QEMU64))
CPU32  := $(if $(QEMU32),, -cpu qemu32)

# --- Audio backend auto-detect (pa -> pipewire -> alsa -> sdl); fallback to SDL ---
AUDIO_DRV ?= $(shell \
  if [ -n "$(QEMU)" ]; then \
    "$(QEMU)" -audio help 2>/dev/null | awk '/Available audio drivers:/{ok=1;next} ok{gsub(",","");print}' \
    | tr ' ' '\n' | grep -E '^(pa|pipewire|alsa|sdl)$$' | head -n1 ; \
  fi)
# Default to SDL if detection failed
ifeq ($(strip $(AUDIO_DRV)),)
  AUDIO_DRV := sdl
  $(warning Kein Audio-Treiber erkannt – verwende SDL als Fallback.)
endif
AUDIODEV := -audiodev $(AUDIO_DRV),id=snd
# Always route both SB16 and PC speaker through the selected audio backend
SOUND := $(AUDIODEV) -device sb16,audiodev=snd -machine pcspk-audiodev=snd

# --- Lauf-Flags ---
RUNFLAGS := -m 1024 -rtc base=localtime,clock=host,driftfix=slew -vga std $(CPU32)
DEBUGCON := -chardev stdio,id=dbg -device isa-debugcon,iobase=0xe9,chardev=dbg -device isa-debug-exit,iobase=0xf4,iosize=0x04 \

# --- Run: 32MB HDD-Image booten, test.txt injizieren/extrahieren ---
run: os-image.bin
	dd if=/dev/zero of=disk_image.img bs=512 count=86016
	dd if=os-image.bin of=disk_image.img conv=notrunc
	# Inject HomebrewDB payload hinter Kernel: DB_START_LBA = 130 + 16384 = 16514
	dd if=database.hbdb of=disk_image.img bs=512 seek=16514 conv=notrunc
	@if [ -z "$(QEMU)" ]; then echo "QEMU fehlt (install qemu-system-x86)"; exit 127; fi
	$(QEMU) $(RUNFLAGS) $(SOUND) $(DEBUGCON) -drive file=disk_image.img,format=raw,if=ide -boot c
	# wenn qemu beendet ist: db zurückholen (Host-Datei aktualisieren)
	dd if=disk_image.img of=database.hbdb bs=512 skip=16514 count=65536 conv=notrunc

# Optional: DB aus Image holen (schreibt database.hbdb!)
db_pull: disk_image.img
	dd if=disk_image.img of=database.hbdb bs=512 skip=16514 count=65536 conv=notrunc

rerun:
	@if [ -z "$(QEMU)" ]; then echo "QEMU fehlt (install qemu-system-x86)"; exit 127; fi
	$(QEMU) $(RUNFLAGS) $(SOUND) $(DEBUGCON) -drive file=disk_image.img,format=raw,if=ide -boot c

echo: os-image.bin
	xxd $<

# --- Debug: GDB stub + hilfreiche Logs ---
kernel.elf: $(COMMON_OBJS) kernel/kernel.o
	ld -nostdlib -m elf_i386 -o $@ -Ttext 0x100000 $^

debug:
	@if [ -z "$(QEMU)" ]; then echo "QEMU fehlt (install qemu-system-x86)"; exit 127; fi
	$(MAKE) DEBUG=1 os-image.bin kernel.elf
	$(QEMU) $(RUNFLAGS) $(SOUND) $(DEBUGCON) -s -S -d guest_errors,int -drive file=disk_image.img,format=raw,if=ide -boot c &
	gdb -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"

test-env:
	@echo "QEMU  : $(QEMU)"; \
	 echo "AUDIO : $(if $(AUDIO_DRV),$(AUDIO_DRV),none)"; \
	 $(if $(QEMU),$(QEMU) -audio help | head -n 80,echo "QEMU not found")

# --- Compile rules ---
%.o: %.c ${HEADERS}
	gcc -Wno-discarded-qualifiers -Wno-implicit-function-declaration -Wno-overflow -fno-PIC -nostdlib --no-pie -m32 -ffreestanding $(CFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf -o $@

%.bin: %.asm
	nasm $< -f bin -o $@

%.dis: %.bin
	ndisasm -b 32 $< > $@

# --- Clean ---
clean:
	$(RM) *.bin *.o *.dis *.elf
	$(RM) kernel/*.o
	$(RM) boot/*.o boot/*.bin
	$(RM) drivers/*.o
	$(RM) cpu/*.o
	$(RM) programs/*.o programs/dobby/*.o
	$(RM) stdlibs/*.o
hbdbtool: tools/hbdbtool.c
	gcc -O2 -o $@ $<

# --- Database (HomebrewDB) ----------------------------------------------------
# Place input files in ./payload and run `make database` to build database.hbdb
PAYLOAD_DIR := payload
PAYLOAD_FILES := $(wildcard $(PAYLOAD_DIR)/*)

# Initialize payload dir and move example assets/scripts into it once
payload-init:
	@mkdir -p $(PAYLOAD_DIR)
	@[ -f logo.bmp ] && mv -f logo.bmp $(PAYLOAD_DIR)/ || true
	@[ -f explosion2.bmp ] && mv -f explosion2.bmp $(PAYLOAD_DIR)/ || true
	@[ -f test.c ] && mv -f test.c $(PAYLOAD_DIR)/ || true
	@[ -f beep.c ] && mv -f beep.c $(PAYLOAD_DIR)/ || true
	@[ -f examples/bgatest.c ] && mv -f examples/bgatest.c $(PAYLOAD_DIR)/bgatest.c || true
	@# If no beep.c present, create a minimal one
	@if [ ! -f $(PAYLOAD_DIR)/beep.c ]; then \
	  echo "main(){ beep(880,200); }" > $(PAYLOAD_DIR)/beep.c; \
	  echo "[payload-init] created minimal beep.c"; \
	fi

# Build database.hbdb from all files in payload directory
database: hbdbtool $(PAYLOAD_FILES)
	@rm -f database.hbdb
	@set -e; for f in $(PAYLOAD_FILES); do \
	  bn=$$(basename "$$f"); \
	  echo "put $$bn"; \
	  ./hbdbtool put database.hbdb "$$bn" "$$f"; \
	done
	@echo "database.hbdb built with $$(ls -1 $(PAYLOAD_DIR) | wc -l) entries"

# Inspect database on host
database-ls: hbdbtool database.hbdb
	./hbdbtool ls database.hbdb

# Remove generated DB file
database-clean:
	$(RM) database.hbdb

# --- Git push helper ---
push:
	@echo "==> cleaning build artefacts..."
	$(MAKE) clean
	rm -f *.bin *.img
	@echo "==> adding changes..."
	git add .
	@echo "==> committing..."
	git commit -m "$(if $(m),$(m),auto-push from make)"
	@echo "==> pushing..."
	git push origin main
