# Run inside `nix develop`, which provides cabin, CPPFLAGS (for limine.h),
# LIMINE_DATA and OVMF_CODE/OVMF_VARS. Cabin builds the kernel (see
# cabin.toml); this file packs it into a bootable ISO and runs QEMU.

PROFILE ?= dev
KERNEL := build/$(PROFILE)/packages/basil/kernel
IMAGE := build/image/$(PROFILE)
ISO := $(IMAGE)/basil.iso

QEMU := qemu-system-x86_64
QEMUFLAGS := -M q35 -m 256M -serial stdio

.PHONY: all kernel iso run run-bios clean FORCE
all: iso
kernel: $(KERNEL)

# Cabin decides what is stale, except the linker script, which it does not
# track; drop the kernel so a script change forces a relink.
$(KERNEL): FORCE
	@if [ -f $@ ] && [ kernel/linker.ld -nt $@ ]; then rm -f $@; fi
	cabin build --profile $(PROFILE)

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

# UEFI boot (default). The vars file must be writable, so keep a copy.
build/image/ovmf-vars.fd:
	@mkdir -p $(@D)
	cp $(OVMF_VARS) $@
	chmod u+w $@

run: $(ISO) build/image/ovmf-vars.fd
	$(QEMU) $(QEMUFLAGS) \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=build/image/ovmf-vars.fd \
		-cdrom $(ISO)

run-bios: $(ISO)
	$(QEMU) $(QEMUFLAGS) -cdrom $(ISO)

clean:
	rm -rf build
