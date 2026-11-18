# SPDX-License-Identifier: GPL-3.0-or-later
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
NM = $(CROSS_COMPILE)nm
PYTHON ?= python3
QEMU ?= qemu-system-i386
GDB ?= gdb
HOST_CC ?= cc
LIBERUMS ?= build/liberums
KERNEL := $(abspath $(LIBERUMS)/build/liberums.elf)
DISPLAY_BACKEND ?= gtk
GDB_PORT ?= 1234

ARCH := -m32 -march=i686 -msoft-float -mno-mmx -mno-sse -mno-sse2
FLAGS := $(ARCH) -std=c11 -Os -g -ffreestanding -fno-builtin -fno-stack-protector \
         -fno-pic -fno-pie -fno-unwind-tables -fno-asynchronous-unwind-tables \
         -ffunction-sections -fdata-sections -Wall -Wextra -Werror
INC := -Iinclude -Isdk/include
BASE := build/start.o build/runtime.o
CORE := build/heap.o build/string.o build/posix.o build/tty.o
EDITOR := $(CORE) build/kilo.o build/kilo_port.o
QEMU_FLAGS := -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
              -serial stdio -monitor none -nic none -no-reboot -no-shutdown
BOOT_OPTIONS := multi-external multi-vga=1 multi-keyboard=1

.PHONY: all editor kernel test integration run debug clean verify-source
all: build/hello.elf editor
editor: build/editor.elf

build:
	mkdir -p $@

build/start.o: arch/x86/start.S Makefile | build
	$(CC) $(ARCH) -fno-pie -g -c $< -o $@

build/%.o: src/%.c Makefile | build
	$(CC) $(INC) $(FLAGS) -MMD -MP -c $< -o $@

build/%.o: tests/%.c Makefile | build
	$(CC) $(INC) $(FLAGS) -MMD -MP -c $< -o $@

build/kilo.c: ports/kilo/upstream/kilo.c ports/kilo/patches/0001-liberums-port.patch ports/kilo/upstream/SHA256SUMS | build
	cd ports/kilo/upstream && sha256sum -c SHA256SUMS
	patch --batch --forward --fuzz=0 -o $@ $< < $(word 2,$^)

build/kilo.o: build/kilo.c Makefile
	$(CC) $(INC) $(FLAGS) -MMD -MP -c $< -o $@

build/editor-oom.o: src/editor_main.c Makefile | build
	$(CC) $(INC) $(FLAGS) -DOOM_FIXTURE -MMD -MP -c $< -o $@

build/hello.elf: $(BASE) build/hello.o
build/editor.elf: $(BASE) $(EDITOR) build/editor_main.o
build/editor-oom.elf: $(BASE) $(EDITOR) build/editor-oom.o
build/guest-test.elf: $(BASE) $(CORE) build/guest-test.o
build/fault-fixture.elf: $(BASE) $(CORE) build/fault-fixture.o

build/%.elf: arch/x86/linker.ld Makefile
	$(LD) -m elf_i386 --gc-sections --build-id=none -Map=$(@:.elf=.map) -T $< $(filter %.o,$^) -o $@
	test "$$(wc -c < $@)" -le 262144
	symbols=$$($(NM) -u "$@") && test -z "$$symbols"

kernel:
	+sh scripts/kernel.sh "$(abspath $(LIBERUMS))" "$(MAKE)"

HOST_NAMES := open close read write lseek fstat ftruncate isatty ioctl errno strerror \
              off_t ssize_t stat winsize termios tcgetattr tcsetattr
HOST_RENAME := $(foreach symbol,$(HOST_NAMES),-D$(symbol)=test_$(symbol))

build/host-test: tests/host-test.c tests/ramfs_cases.h src/posix.c src/string.c $(wildcard include/*.h) Makefile | build
	$(HOST_CC) -std=c11 -g -fno-builtin -Wall -Wextra -Werror $(HOST_RENAME) -Iinclude \
		-fsanitize=undefined -fno-sanitize-recover=undefined tests/host-test.c src/posix.c src/string.c -o $@

verify-source:
	cd ports/kilo/upstream && sha256sum -c SHA256SUMS
	$(PYTHON) scripts/verify_sdk.py

test: verify-source all build/host-test
	build/host-test
	$(PYTHON) tests/elf_layout.py build/hello.elf build/editor.elf

integration: test kernel build/guest-test.elf build/fault-fixture.elf build/editor-oom.elf
	$(PYTHON) tests/integration.py --kernel "$(KERNEL)" --qemu "$(QEMU)" --gdb "$(GDB)"

run: editor kernel
	$(QEMU) $(QEMU_FLAGS) -display "$(DISPLAY_BACKEND)" -kernel "$(KERNEL)" \
		-append "$(BOOT_OPTIONS)" -initrd build/editor.elf

debug: editor kernel
	$(QEMU) $(QEMU_FLAGS) -display "$(DISPLAY_BACKEND)" -kernel "$(KERNEL)" \
		-append "$(BOOT_OPTIONS)" -initrd build/editor.elf -S -gdb tcp:127.0.0.1:$(GDB_PORT)

clean:
	rm -rf build

-include $(wildcard build/*.d)
