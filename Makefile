# $@ = target file
# $< = first dependency
# $^ = all dependencies

.PHONY: all run rerun echo debug clean test-env

# --- Quellen ---
C_SOURCES = $(wildcard kernel/*.c drivers/*.c cpu/*.c stdlibs/*.c programs/*.c programs/dobby/*.c)
HEADERS   = $(wildcard kernel/*.h  drivers/*.h cpu/*.h stdlibs/*.h programs/*.h programs/dobby/*.h)

# --- Default ---
all: run

# --- Revision bump (wie gehabt) ---
CURRENT_REVISION := $(shell cat version.txt)
CURRENT_DATE     := $(shell date)
NEW_REVISION     := $(shell expr $(CURRENT_REVISION) + 1)
$(shell echo $(NEW_REVISION) > version.txt)

CFLAGS := -DREVISION_NUMBER=$(NEW_REVISION) -DREVISION_DATE='"$(CURRENT_DATE)"'

# --- Objektlisten ---
COMMON_OBJS = boot/kernel_entry.o \
              kernel/bga_minimal.o \
              cpu/interrupt.o cpu/setjmp.o cpu/longjmp.o \
              kernel/conio.o kernel/kernel_command.o kernel/math.o kernel/fpu.o \
              kernel/mem.o kernel/time.o kernel/util.o kernel/perf.o \
              drivers/debug.o drivers/display.o drivers/hdd.o drivers/hidden_cmd.o \
              drivers/keyboard.o drivers/mouse.o drivers/ports.o drivers/video.o drivers/dma.o \
              cpu/idt.o cpu/isr.o cpu/timer.o \
              stdlibs/file.o stdlibs/memory.o stdlibs/stdio.o stdlibs/string.o \
              stdlibs/textui.o stdlibs/tinysql.o \
              programs/editor.o programs/hexviewer.o \
              programs/snake.o programs/snaketext.o \
              programs/sb16demo.o \
              programs/dobby/lclib.o programs/dobby/littlec.o programs/dobby/parser.o

COMMON_OBJS_STUB = boot/kernel_entry.o \
              cpu/interrupt.o cpu/setjmp.o cpu/longjmp.o \
              kernel/conio.o kernel/math.o \
              kernel/mem.o kernel/time.o kernel/util.o \
              drivers/debug.o drivers/display.o drivers/hdd.o drivers/hidden_cmd.o \
              drivers/keyboard.o drivers/ports.o drivers/video.o \
              cpu/idt.o cpu/isr.o cpu/timer.o \
              stdlibs/file.o stdlibs/memory.o stdlibs/stdio.o stdlibs/string.o

# --- Build Kernel Stub + Kernel ---
kernel_stub.bin: $(COMMON_OBJS_STUB) kernel/kernel_stub.o
	ld -nostdlib -m elf_i386 -o $@ -Ttext 0x10000 $^ --oformat binary

kernel.bin: $(COMMON_OBJS) kernel/kernel.o
	ld -nostdlib -m elf_i386 -o $@ -Ttext 0x100000 $^ --oformat binary

# --- Image bauen mit festen Offsets ---
os-image.bin: boot/mbr.bin kernel_stub.bin kernel.bin
	# 32MB-Image vorbereiten
	dd if=/dev/zero of=$@ bs=512 count=65536
	# MBR nach Sektor 0
	dd if=boot/mbr.bin of=$@ conv=notrunc bs=512 seek=0
	# Stub ab Sektor 1
	dd if=kernel_stub.bin of=$@ conv=notrunc bs=512 seek=1
	# 2 Sektoren Puffer (128–129) bleiben leer
	# Großer Kernel ab Sektor 130
	dd if=kernel.bin of=$@ conv=notrunc bs=512 seek=130

# --- QEMU detect + CPU (32-bit Kernel auf 64-bit QEMU erzwingen) ---
QEMU32 := $(shell command -v qemu-system-i386 2>/dev/null)
QEMU64 := $(shell command -v qemu-system-x86_64 2>/dev/null)
QEMU   := $(if $(QEMU32),$(QEMU32),$(QEMU64))
CPU32  := $(if $(QEMU32),, -cpu qemu32)

# --- Audio backend auto-detect (pa -> pipewire -> alsa -> sdl) ---
AUDIO_DRV := $(shell \
  if [ -n "$(QEMU)" ]; then \
    "$(QEMU)" -audio help 2>/dev/null | awk '/Available audio drivers:/{ok=1;next} ok{gsub(",","");print}' \
    | tr ' ' '\n' | grep -E '^(pa|pipewire|alsa|sdl)$$' | head -n1 ; \
  fi)

ifeq ($(strip $(AUDIO_DRV)),)
  $(warning Kein QEMU-Audio-Treiber erkannt. Starte mit stummem SB16.)
  SOUND := -device sb16
else
  AUDIODEV := -audiodev $(AUDIO_DRV),id=snd
  SOUND := $(AUDIODEV) -device sb16,audiodev=snd -machine pcspk-audiodev=snd
endif

# --- Lauf-Flags ---
RUNFLAGS := -m 1024 -rtc base=2023-08-03T12:34:56 -vga std $(CPU32)
DEBUGCON := -chardev stdio,id=dbg -device isa-debugcon,iobase=0xe9,chardev=dbg

# --- Run: 32MB HDD-Image booten, test.pc injizieren/extrahieren ---
run: os-image.bin
	dd if=/dev/zero of=disk_image.img bs=512 count=65536
	dd if=os-image.bin of=disk_image.img conv=notrunc
	dd if=test.pc of=disk_image.img bs=1 seek=1048576 conv=notrunc
	@if [ -z "$(QEMU)" ]; then echo "QEMU fehlt (install qemu-system-x86)"; exit 127; fi
	$(QEMU) $(RUNFLAGS) $(SOUND) $(DEBUGCON) -drive file=disk_image.img,format=raw,if=ide -boot c
	dd if=disk_image.img of=test.pc bs=1 skip=1048576 count=951

rerun:
	@if [ -z "$(QEMU)" ]; then echo "QEMU fehlt (install qemu-system-x86)"; exit 127; fi
	$(QEMU) $(RUNFLAGS) $(SOUND) $(DEBUGCON) -drive file=disk_image.img,format=raw,if=ide -boot c

echo: os-image.bin
	xxd $<

# --- Debug: GDB stub + hilfreiche Logs ---
kernel.elf: $(COMMON_OBJS) kernel/kernel.o
	ld -nostdlib -m elf_i386 -o $@ -Ttext 0x100000 $^

debug: os-image.bin kernel.elf
	@if [ -z "$(QEMU)" ]; then echo "QEMU fehlt (install qemu-system-x86)"; exit 127; fi
	$(QEMU) $(RUNFLAGS) $(SOUND) $(DEBUGCON) -s -S -d guest_errors,int -drive file=disk_image.img,format=raw,if=ide -boot c &
	gdb -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"

test-env:
	@echo "QEMU  : $(QEMU)"; \
	 echo "AUDIO : $(if $(AUDIO_DRV),$(AUDIO_DRV),none)"; \
	 $(if $(QEMU),$(QEMU) -audio help | sed -n '1,80p',echo "QEMU not found")

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
