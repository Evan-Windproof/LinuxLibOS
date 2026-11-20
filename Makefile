# SPDX-License-Identifier: GPL-3.0-or-later
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
NM = $(CROSS_COMPILE)nm
READELF = $(CROSS_COMPILE)readelf
OBJDUMP = $(CROSS_COMPILE)objdump
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
EDITOR := $(CORE) build/app_call.o build/application.o build/kilo.o build/kilo_port.o
CAT_PORT := ports/sbase-cat
CAT_PATCH := $(CAT_PORT)/patches/0001-liberums-port.patch
CAT_FILES := cat.c libutil/concat.c libutil/writeall.c
CAT_OBJECTS := $(addprefix build/sbase-cat/,$(CAT_FILES:.c=.o))
QEMU_FLAGS := -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
              -serial stdio -monitor none -nic none -no-reboot -no-shutdown
BOOT_OPTIONS := multi-external multi-vga=1 multi-keyboard=1

.PHONY: all editor apps kernel test integration run run-launcher debug clean verify-source
.PHONY: linux-elf-tests linux-loader-test run-linux-loader
all: build/hello.elf apps build/linux-loader.elf
editor: build/editor.elf
apps: editor build/launcher.elf

build:
	mkdir -p $@

build/%.o: arch/x86/%.S Makefile | build
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

$(addprefix build/sbase-cat/,$(CAT_FILES) sbase_cat_internal.h) &: $(CAT_PATCH) $(wildcard $(CAT_PORT)/upstream/* $(CAT_PORT)/upstream/libutil/*) Makefile | build
	cd $(CAT_PORT)/upstream && sha256sum -c SHA256SUMS
	cp -R $(CAT_PORT)/upstream/. build/sbase-cat
	rm -f build/sbase-cat/sbase_cat_internal.h
	patch --batch --forward --fuzz=0 -d build/sbase-cat -p1 < $(CAT_PATCH)

build/sbase-cat/%.o: build/sbase-cat/%.c Makefile
	$(CC) $(INC) $(FLAGS) -MMD -MP -c $< -o $@

build/editor-oom.o: APP_FLAGS := -DOOM_FIXTURE
build/editor-long.o: APP_FLAGS := -DLONG_FIXTURE
build/launcher.o: APP_FLAGS := -DAPP_LAUNCHER
build/editor-oom.o build/editor-long.o build/launcher.o: src/editor_main.c Makefile | build
	$(CC) $(INC) $(FLAGS) $(APP_FLAGS) -MMD -MP -c $< -o $@

build/linux-sample.o: tests/fixtures/linux-sample.S Makefile | build
	$(AS) --32 $< -o $@

build/linux-sample.elf: build/linux-sample.o Makefile
	$(LD) -m elf_i386 -static -nostdlib --build-id=none -s $< -o $@

build/linux-sample.txt: build/linux-sample.elf
	sha256sum $< > $@
	$(READELF) -h -lW -d -n $< >> $@
	$(OBJDUMP) -d $< >> $@

build/linux_sample_blob.o: src/linux_sample_blob.S build/linux-sample.elf Makefile
	$(CC) $(ARCH) -fno-pie -g -c $< -o $@

build/linux-loader.elf: $(BASE) build/linux_loader_main.o build/linux_sample_blob.o \
                        build/linux_elf32.o build/linux_guest.o build/string.o

build/hello.elf: $(BASE) build/hello.o
build/editor.elf: $(BASE) $(EDITOR) build/editor_main.o
build/editor-oom.elf: $(BASE) $(EDITOR) build/editor-oom.o
build/editor-long.elf: $(BASE) $(EDITOR) build/editor-long.o
build/launcher.elf: $(BASE) $(EDITOR) $(CAT_OBJECTS) build/launcher.o
build/guest-test.elf: $(BASE) $(CORE) build/guest-test.o
build/fault-fixture.elf: $(BASE) $(CORE) build/fault-fixture.o

build/%.elf: arch/x86/linker.ld Makefile
	$(LD) -m elf_i386 --gc-sections --build-id=none -Map=$(@:.elf=.map) -T $< $(filter %.o,$^) -o $@
	test "$$(wc -c < $@)" -le 262144
	symbols=$$($(NM) -u "$@") && test -z "$$symbols"

kernel:
	+sh scripts/kernel.sh "$(abspath $(LIBERUMS))" "$(MAKE)"

HOST_NAMES := open close read write lseek fstat ftruncate isatty ioctl errno strerror \
              off_t ssize_t stat unlink access rename winsize termios tcgetattr tcsetattr
HOST_RENAME := $(foreach symbol,$(HOST_NAMES),-D$(symbol)=test_$(symbol))

build/host-test: tests/host-test.c tests/ramfs_cases.h src/posix.c src/string.c $(wildcard include/*.h) Makefile | build
	$(HOST_CC) -std=c11 -g -fno-builtin -Wall -Wextra -Werror $(HOST_RENAME) -Iinclude \
		-fsanitize=undefined -fno-sanitize-recover=undefined tests/host-test.c src/posix.c src/string.c -o $@

verify-source:
	cd ports/kilo/upstream && sha256sum -c SHA256SUMS
	cd $(CAT_PORT)/upstream && sha256sum -c SHA256SUMS
	$(PYTHON) scripts/verify_sdk.py

build/linux_elf_test: tests/linux_elf_test.c src/linux_elf32.c src/linux_guest.c \
                      src/linux_elf32.h include/linux_guest.h include/libos.h Makefile | build
	$(HOST_CC) -std=c11 -g -Wall -Wextra -Werror -Iinclude \
		-fsanitize=undefined -fno-sanitize-recover=undefined $(filter %.c,$^) -o $@

linux-elf-tests: build/linux_elf_test build/linux-sample.txt
	build/linux_elf_test build/linux-sample.elf

test: verify-source all build/host-test linux-elf-tests
	build/host-test
	$(PYTHON) tests/elf_layout.py build/hello.elf build/editor.elf build/launcher.elf build/linux-loader.elf

integration: test kernel build/guest-test.elf build/fault-fixture.elf build/editor-oom.elf build/editor-long.elf
	$(PYTHON) tests/integration.py --kernel "$(KERNEL)" --qemu "$(QEMU)" --gdb "$(GDB)"

linux-loader-test: linux-elf-tests build/linux-loader.elf kernel
	$(PYTHON) tests/integration.py --kernel "$(KERNEL)" --qemu "$(QEMU)" --gdb "$(GDB)" --scenario linux-loader

run-linux-loader: build/linux-loader.elf kernel
	$(QEMU) $(QEMU_FLAGS) -display none -kernel "$(KERNEL)" \
		-append multi-external -initrd build/linux-loader.elf

run: editor kernel
	$(QEMU) $(QEMU_FLAGS) -display "$(DISPLAY_BACKEND)" -kernel "$(KERNEL)" \
		-append "$(BOOT_OPTIONS)" -initrd build/editor.elf

run-launcher: apps kernel
	$(QEMU) $(QEMU_FLAGS) -display "$(DISPLAY_BACKEND)" -kernel "$(KERNEL)" \
		-append "$(BOOT_OPTIONS)" -initrd build/launcher.elf

debug: editor kernel
	$(QEMU) $(QEMU_FLAGS) -display "$(DISPLAY_BACKEND)" -kernel "$(KERNEL)" \
		-append "$(BOOT_OPTIONS)" -initrd build/editor.elf -S -gdb tcp:127.0.0.1:$(GDB_PORT)

clean:
	rm -rf build

-include $(wildcard build/*.d build/sbase-cat/*.d build/sbase-cat/libutil/*.d)
