# Run inside `nix develop`, which provides cabin, CPPFLAGS (for limine.h),
# LIMINE_DATA and OVMF_CODE/OVMF_VARS. Cabin builds the kernel (see
# cabin.toml); this file packs it into a bootable ISO and runs QEMU.

PROFILE ?= dev
KERNEL := build/$(PROFILE)/packages/basil/kernel
IMAGE := build/image/$(PROFILE)
ISO := $(IMAGE)/basil.iso

QEMU := qemu-system-x86_64
# KVM when available (/dev/kvm readable and writable), else software emulation.
QEMUFLAGS := -M q35 -m 256M -serial stdio -accel kvm -accel tcg

.PHONY: all kernel iso run run-bios test test-host test-kernel clean FORCE
all: iso
kernel: $(KERNEL)

# Cabin decides what is stale, except the linker script, which it does not
# track; drop the kernel so a script change forces a relink.
$(KERNEL): FORCE
	@if [ -f $@ ] && [ kernel/linker.ld -nt $@ ]; then rm -f $@; fi
	cabin build --profile $(PROFILE)
	$(call merge_compdb,kernel)

test: test-host test-kernel

# Host unit tests (libk/tests); they run on Linux, not in QEMU.
test-host:
	cabin test --workspace --profile $(PROFILE); status=$$?; \
		$(call merge_compdb,test); exit $$status

# In-kernel tests: boot a `test`-profile kernel, which runs every KTEST and
# reports through QEMU's isa-debug-exit device: (0x10 << 1) | 1 = 33 means
# all passed. -no-reboot turns a triple fault into a failure instead of a
# boot loop; the timeout catches a hang.
TEST_QEMUFLAGS := -device isa-debug-exit,iobase=0xf4,iosize=0x04 -display none -no-reboot
TEST_TIMEOUT ?= 60
test-kernel:
	$(MAKE) --no-print-directory PROFILE=test iso
	timeout $(TEST_TIMEOUT) $(QEMU) $(QEMUFLAGS) $(TEST_QEMUFLAGS) -cdrom build/image/test/basil.iso; \
		status=$$?; \
		if [ $$status -eq 33 ]; then exit 0; fi; \
		if [ $$status -eq 124 ]; then echo "kernel tests timed out after $(TEST_TIMEOUT)s"; \
		else echo "kernel tests failed (QEMU exit status $$status)"; fi; \
		exit 1

# Cabin rewrites compile_commands.json with only the last command's targets,
# so keep each command's entries and hand clangd their union.
COMPDB := build/$(PROFILE)/compile_commands.json
merge_compdb = cp $(COMPDB) $(COMPDB:.json=.$(1).json) && \
	jq -s 'add | unique_by(.file)' build/$(PROFILE)/compile_commands.*.json > $(COMPDB)

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

# UEFI boot (default). The vars file must be writable, so keep a copy; it
# persists between runs (delete it to reset the firmware settings).
build/image/ovmf-vars.fd:
	@mkdir -p $(@D)
	cp $(OVMF_VARS) $@
	chmod u+w $@

# Firmware splash: if LOGO exists, boot a copy of OVMF with the TianoCore
# logo swapped for it (BMP, PNG or JPG); otherwise boot the stock firmware.
LOGO ?= assets/logo.png
ifneq ($(wildcard $(LOGO)),)
OVMF_RUN_CODE := build/image/ovmf-code.fd
else
OVMF_RUN_CODE := $(OVMF_CODE)
endif

build/image/ovmf-code.fd: $(LOGO)
	@mkdir -p $(@D)
	boot-logo replace $< $(OVMF_CODE) --output $@

run: $(ISO) build/image/ovmf-vars.fd $(filter build/%,$(OVMF_RUN_CODE))
	$(QEMU) $(QEMUFLAGS) \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_RUN_CODE) \
		-drive if=pflash,format=raw,file=build/image/ovmf-vars.fd \
		-cdrom $(ISO)

run-bios: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $(ISO)

clean:
	rm -rf build
