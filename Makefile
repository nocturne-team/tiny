# tiny — bare-metal x86_64 system info tool. See README.md.

CC      := clang
LD      := ld.lld
NASM    := nasm
HOSTCC  := gcc
QEMU    := qemu-system-x86_64

BUILD   ?= build
KDEFS   ?=

LIMINE_TAG := v11.4.1-binary
LIMINE_DIR := limine
OVMF_DIR   := /usr/share/edk2/OvmfX64

KCFLAGS := --target=x86_64-unknown-none-elf -std=gnu11 -O2 -g -pipe \
	-Wall -Wextra -Werror=implicit-function-declaration \
	-ffreestanding -fno-stack-protector -fno-stack-check -fno-PIC \
	-fno-omit-frame-pointer -mno-red-zone -mcmodel=kernel -mgeneral-regs-only \
	-Isrc $(KDEFS)
KLDFLAGS  := -nostdlib -static -z max-page-size=0x1000 -T linker.ld
NASMFLAGS := -f elf64 -g
HOSTCFLAGS := -std=gnu11 -O0 -g -Wall -Wextra -Isrc -Itests -fsanitize=address,undefined

CSRCS  := $(shell find src -name '*.c')
ASRCS  := $(shell find src -name '*.asm')
OBJS   := $(CSRCS:%.c=$(BUILD)/%.o) $(ASRCS:%.asm=$(BUILD)/%.asm.o)
KERNEL := $(BUILD)/kernel.elf
ISO    := $(BUILD)/tiny.iso

QEMUFLAGS := -M q35 -m 512M -serial stdio -no-reboot -device qemu-xhci,id=xhci

.PHONY: all iso run run-uefi shots test smoke clean distclean
all: $(ISO)
iso: $(ISO)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(KCFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(KLDFLAGS) $(OBJS) -o $@

$(LIMINE_DIR)/limine:
	rm -rf $(LIMINE_DIR)
	git clone --depth=1 --branch=$(LIMINE_TAG) https://github.com/Limine-Bootloader/Limine.git $(LIMINE_DIR)
	$(MAKE) -C $(LIMINE_DIR) CC=$(HOSTCC)

$(ISO): $(KERNEL) limine.conf $(LIMINE_DIR)/limine
	rm -rf $(BUILD)/iso_root
	mkdir -p $(BUILD)/iso_root/boot/limine $(BUILD)/iso_root/EFI/BOOT
	cp $(KERNEL) $(BUILD)/iso_root/boot/kernel.elf
	cp limine.conf $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin \
		$(LIMINE_DIR)/limine-uefi-cd.bin $(BUILD)/iso_root/boot/limine/
	cp $(LIMINE_DIR)/BOOTX64.EFI $(BUILD)/iso_root/EFI/BOOT/
	xorriso -as mkisofs -quiet -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(BUILD)/iso_root -o $@
	$(LIMINE_DIR)/limine bios-install $@

run: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $(ISO) -boot d

$(BUILD)/ovmf-vars.fd:
	@mkdir -p $(BUILD)
	cp $(OVMF_DIR)/OVMF_VARS.fd $@

run-uefi: $(ISO) $(BUILD)/ovmf-vars.fd
	$(QEMU) $(QEMUFLAGS) \
		-drive if=pflash,unit=0,format=raw,readonly=on,file=$(OVMF_DIR)/OVMF_CODE.fd \
		-drive if=pflash,unit=1,format=raw,file=$(BUILD)/ovmf-vars.fd \
		-cdrom $(ISO) -boot d

shots: $(ISO)
	tools/qemu-shot.py $(ISO) build/shots shot:cpu right shot:memory pgdn shot:memory-pgdn \
		f3 shot:pci f4 shot:firmware end shot:firmware-end f5 shot:about pgdn shot:about-pgdn s shot:confirm-shutdown esc shot:cancelled
	tools/qemu-shot.py $(ISO) build/shots --uefi f3 shot:uefi-pci f4 shot:uefi-firmware

# --- host unit tests ----------------------------------------------------------
TESTS :=
TESTS += test_fmt
deps_test_fmt := src/lib/fmt.c
TESTS += test_time
deps_test_time := src/lib/time.c src/lib/fmt.c
TESTS += test_layout
deps_test_layout := src/gfx/layout.c
TESTS += test_ps2
deps_test_ps2 := src/input/ps2_decode.c
TESTS += test_cpu
deps_test_cpu := src/info/cpu_decode.c
TESTS += test_mem
deps_test_mem := src/info/mem.c
TESTS += test_acpi
deps_test_acpi := src/info/acpi.c src/info/physmap.c src/lib/fmt.c
TESTS += test_smbios
deps_test_smbios := src/info/smbios.c src/info/physmap.c src/lib/fmt.c
TESTS += test_page test_tabs
deps_test_page := src/ui/page.c src/lib/fmt.c
deps_test_tabs := src/info/edid.c src/info/smbios.c src/info/physmap.c src/ui/tab_about.c src/ui/tab_pci.c src/info/pci.c src/ui/tab_cpu.c src/ui/tab_mem.c src/ui/tab_fw.c src/ui/page.c src/info/mem.c src/lib/fmt.c
TESTS += test_pci
deps_test_pci := src/info/pci.c
TESTS += test_braille
deps_test_braille := src/gfx/braille.c
TESTS += test_physmap
deps_test_physmap := src/info/physmap.c
TESTS += test_edid
deps_test_edid := src/info/edid.c src/lib/fmt.c
TESTS += test_pool
deps_test_pool := src/mm/pool.c
TESTS += test_hid_kbd
deps_test_hid_kbd := src/usb/hid_kbd.c
TESTS += test_usb_desc
deps_test_usb_desc := src/usb/usb_desc.c
TESTS += test_xhci_calc
deps_test_xhci_calc := src/usb/xhci_calc.c src/lib/fmt.c
# (add "TESTS += test_x" and "deps_test_x := ..." lines above this comment)

define TEST_RULE
$(BUILD)/tests/$(1): tests/$(1).c $$(deps_$(1)) tests/test.h
	@mkdir -p $$(dir $$@)
	$$(HOSTCC) $$(HOSTCFLAGS) tests/$(1).c $$(deps_$(1)) -o $$@
endef
$(foreach t,$(TESTS),$(eval $(call TEST_RULE,$(t))))

test: $(TESTS:%=$(BUILD)/tests/%)
	@for t in $^; do ./$$t || exit 1; done
	@echo "all unit tests passed"

# --- QEMU smoke tests ---------------------------------------------------------
SMOKE_EXPECT :=
SMOKE_EXPECT += "ps2: keyboard controller ok"
SMOKE_EXPECT += "tsc: " "cpu: "
SMOKE_EXPECT += "MiB available"
SMOKE_EXPECT += "acpi: rev" "smbios: QEMU"
SMOKE_EXPECT += "power: PM1a_CNT"
SMOKE_EXPECT += "pci: "
SMOKE_EXPECT += "memory devices"
SMOKE_EXPECT += "edid: "
SMOKE_EXPECT += "pages: "
SMOKE_EXPECT += "xhci: .* running" "usb: 1 xHCI controller, 0 keyboards"
# (add "SMOKE_EXPECT += \"pattern\"" lines above this comment)

smoke:
	$(MAKE) --no-print-directory BUILD=build/smoke KDEFS=-DSMOKE iso
	tests/smoke.sh build/smoke/tiny.iso bios ok q35 $(SMOKE_EXPECT)
	tests/smoke.sh build/smoke/tiny.iso uefi ok q35 $(SMOKE_EXPECT)
	$(MAKE) --no-print-directory BUILD=build/smoke-panic KDEFS="-DSMOKE -DSMOKE_PANIC" iso
	tests/smoke.sh build/smoke-panic/tiny.iso bios panic
	tests/smoke.sh build/smoke/tiny.iso bios ok q35,i8042=off "ps2: no controller"
	QEMU_EXTRA="-device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-tablet,bus=xhci.0" \
		tests/smoke.sh build/smoke/tiny.iso bios ok q35,i8042=off "keyboard 0627:" "class 00, ignored" "controller, 1 keyboard"
	tests/smoke.sh build/smoke/tiny.iso bios ok q35,acpi=off "acpi: rev 0"
	$(MAKE) --no-print-directory BUILD=build/smoke-noacpi KDEFS="-DSMOKE -DSMOKE_NO_ACPI" iso
	tests/smoke.sh build/smoke-noacpi/tiny.iso bios ok q35 "acpi: not provided"
	$(MAKE) --no-print-directory BUILD=build/smoke-off KDEFS="-DSMOKE -DSMOKE_SHUTDOWN" iso
	tests/smoke.sh build/smoke-off/tiny.iso bios shutdown
	tests/smoke.sh build/smoke-off/tiny.iso uefi shutdown

clean:
	rm -rf build

distclean: clean
	rm -rf $(LIMINE_DIR)

-include $(OBJS:.o=.d)
