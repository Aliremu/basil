PROFILE ?= dev
LOGO ?= assets/logo.png
TEST_TIMEOUT ?= 60
WIN_QEMU ?= /mnt/c/Program Files/qemu/qemu-system-x86_64.exe

ifdef PULSE_SERVER
AUDIO ?= pa,server=$(PULSE_SERVER)
else
AUDIO ?= pa
endif

KERNEL := build/$(PROFILE)/packages/basil/kernel
IMAGE := build/image/$(PROFILE)
ISO := $(IMAGE)/basil.iso
USB := $(IMAGE)/basil.img
COMPDB := build/$(PROFILE)/compile_commands.json
OVMF_RUN_VARS := build/image/ovmf-vars.fd

ifneq ($(wildcard $(LOGO)),)
OVMF_RUN_CODE := build/image/ovmf-code.fd
else
OVMF_RUN_CODE := $(OVMF_CODE)
endif
UEFI_DEPS := $(ISO) $(OVMF_RUN_VARS) $(filter build/%,$(OVMF_RUN_CODE))

QEMU := qemu-system-x86_64
QEMUFLAGS := -M q35 -m 1G -serial stdio -accel kvm -accel tcg
AUDIOFLAGS := -audiodev $(AUDIO),id=snd0 -machine pcspk-audiodev=snd0
TEST_QEMUFLAGS := -device isa-debug-exit,iobase=0xf4,iosize=0x04 -display none -no-reboot
WIN_QEMUFLAGS := -audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0 -M q35 -m 1G -serial stdio
WIN_ENV := LANG=en_US WSLENV="$$WSLENV:LANG"

export CPPFLAGS += -fmacro-prefix-map=$(CURDIR)/=

winpath = "$$(wslpath -w $(1))"
merge_compdb = cp $(COMPDB) $(COMPDB:.json=.$(1).json) && \
	jq -s 'add | unique_by(.file)' build/$(PROFILE)/compile_commands.*.json > $(COMPDB)

.PHONY: all kernel iso usb run run-bios run-win run-win-bios test test-host test-kernel fmt clean FORCE

all: iso

kernel: $(KERNEL)

$(KERNEL): FORCE
	@if [ -f $@ ] && [ kernel/linker.ld -nt $@ ]; then rm -f $@; fi
	cabin build --profile $(PROFILE)
	$(call merge_compdb,kernel)

iso: $(ISO)

$(ISO): $(KERNEL) limine.conf
	rm -rf $(IMAGE)/iso_root
	mkdir -p $(IMAGE)/iso_root/boot/limine $(IMAGE)/iso_root/EFI/BOOT
	cp $(KERNEL) $(IMAGE)/iso_root/boot/kernel
	cp limine.conf $(IMAGE)/iso_root/boot/limine/
	cp $(LIMINE_DATA)/limine-bios.sys $(LIMINE_DATA)/limine-bios-cd.bin \
		$(LIMINE_DATA)/limine-uefi-cd.bin $(IMAGE)/iso_root/boot/limine/
	cp $(LIMINE_DATA)/BOOTX64.EFI $(IMAGE)/iso_root/EFI/BOOT/
	chmod -R u+w $(IMAGE)/iso_root
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(IMAGE)/iso_root -o $@ 2>/dev/null
	limine bios-install $@ >/dev/null 2>&1

usb: $(USB)

$(USB): $(KERNEL) limine.conf
	rm -f $@
	truncate -s 64M $@
	sgdisk $@ -n 1:2048:0 -t 1:ef00 -m 1 >/dev/null
	limine bios-install $@ >/dev/null 2>&1
	mformat -i $@@@1M -F -v BASIL ::
	mmd -i $@@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $@@@1M $(KERNEL) ::/boot/kernel
	mcopy -i $@@@1M limine.conf $(LIMINE_DATA)/limine-bios.sys ::/boot/limine
	mcopy -i $@@@1M $(LIMINE_DATA)/BOOTX64.EFI ::/EFI/BOOT

$(OVMF_RUN_VARS):
	@mkdir -p $(@D)
	cp $(OVMF_VARS) $@
	chmod u+w $@

build/image/ovmf-code.fd: $(LOGO)
	@mkdir -p $(@D)
	boot-logo replace $< $(OVMF_CODE) --output $@

run: $(UEFI_DEPS)
	$(QEMU) $(QEMUFLAGS) $(AUDIOFLAGS) \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_RUN_CODE) \
		-drive if=pflash,format=raw,file=$(OVMF_RUN_VARS) \
		-cdrom $(ISO)

run-bios: $(ISO)
	$(QEMU) $(QEMUFLAGS) $(AUDIOFLAGS) -cdrom $(ISO)

run-win: $(UEFI_DEPS)
	$(WIN_ENV) "$(WIN_QEMU)" $(WIN_QEMUFLAGS) -accel tcg \
		-drive if=pflash,format=raw,readonly=on,file=$(call winpath,$(OVMF_RUN_CODE)) \
		-drive if=pflash,format=raw,file=$(call winpath,$(OVMF_RUN_VARS)) \
		-cdrom $(call winpath,$(ISO))

run-win-bios: $(ISO)
	$(WIN_ENV) "$(WIN_QEMU)" $(WIN_QEMUFLAGS) -accel whpx,kernel-irqchip=off -accel tcg \
		-cdrom $(call winpath,$(ISO))

test: test-host test-kernel

test-host:
	GTEST_COLOR=yes cabin test --workspace --profile $(PROFILE); status=$$?; \
		$(call merge_compdb,test); exit $$status

test-kernel:
	$(MAKE) --no-print-directory PROFILE=test iso
	timeout --foreground $(TEST_TIMEOUT) $(QEMU) $(QEMUFLAGS) $(TEST_QEMUFLAGS) -cdrom build/image/test/basil.iso; \
		status=$$?; \
		if [ $$status -eq 33 ]; then exit 0; fi; \
		if [ $$status -eq 124 ]; then echo "kernel tests timed out after $(TEST_TIMEOUT)s"; \
		else echo "kernel tests failed (QEMU exit status $$status)"; fi; \
		exit 1

fmt:
	cabin fmt --workspace

clean:
	rm -rf build
