# basil

An x86-64 hobby kernel written in C++26, booted by [Limine](https://github.com/limine-bootloader/limine) and built with GCC trunk and [Cabin](https://github.com/Aliremu/cabin).

- `kernel/` is the freestanding kernel ELF and its in-kernel tests (`KTEST`).
- `libk/` is a header-only library of hardware-free kernel code, unit-tested on the host with GoogleTest.

## Getting started

Everything runs inside the Nix dev shell, which provides the toolchain, QEMU, OVMF and Limine:

```sh
nix develop
make run
```

An interactive `nix develop` drops into fish with a Starship prompt. Set `BASIL_NO_FISH=1` to stay in bash, or `BASIL_STARSHIP_CONFIG` to use a different prompt config. `nix develop --command ...` always uses bash.

## Make targets

| Target | Description |
| --- | --- |
| `make` / `make iso` | Build the kernel and pack it into a hybrid BIOS/UEFI ISO at `build/image/<profile>/basil.iso`. |
| `make kernel` | Build only the kernel ELF with Cabin. |
| `make usb` | Build a raw GPT disk image for a USB stick at `build/image/<profile>/basil.img`. |
| `make run` | Boot the ISO in QEMU with UEFI (OVMF). |
| `make run-bios` | Boot the ISO in QEMU with legacy BIOS. |
| `make run-win` | WSL only: boot with UEFI in the Windows build of QEMU. |
| `make run-win-bios` | WSL only: boot with BIOS in the Windows build of QEMU, using the Windows Hypervisor Platform. |
| `make test` | Run `test-host` and `test-kernel`. |
| `make test-host` | Run libk's GoogleTest suites on the host. |
| `make test-kernel` | Boot a `test`-profile kernel headless in QEMU and run every `KTEST`. |
| `make fmt` | Format the workspace with clang-format. |
| `make clean` | Delete `build/`. |

## Variables

| Variable | Default | Description |
| --- | --- | --- |
| `PROFILE` | `dev` | Cabin profile to build: `dev`, `debug` or `test`. |
| `LOGO` | `assets/logo.png` | Firmware splash image (BMP, PNG or JPG) patched into OVMF for `run` and `run-win`. If the file does not exist, stock OVMF is used. |
| `AUDIO` | `pa,server=$PULSE_SERVER` | QEMU `-audiodev` backend for the PC speaker in `run` and `run-bios`. Falls back to plain `pa` when `PULSE_SERVER` is unset; use `none` to mute. |
| `TEST_TIMEOUT` | `60` | Seconds before `test-kernel` gives up on a hung kernel. |
| `WIN_QEMU` | `/mnt/c/Program Files/qemu/qemu-system-x86_64.exe` | Windows QEMU used by `run-win` and `run-win-bios`. |

## Profiles

| Profile | Output | Description |
| --- | --- | --- |
| `dev` | `build/dev/` | `-O2` with debug info. The kernel is too slow to use unoptimized. |
| `debug` | `build/debug/` | `-O0`, for stepping through libk's host tests in gdb. |
| `test` | `build/test/` | Like `dev`, but defines `KERNEL_TESTS`, so `kmain` runs every `KTEST` instead of booting normally. |

## Booting

- QEMU uses KVM when `/dev/kvm` is usable and falls back to TCG otherwise.
- The PC speaker plays through PulseAudio. Under WSLg, `PULSE_SERVER` points at WSLg's server; QEMU ignores that variable, so the Makefile passes it explicitly.
- UEFI variables persist in `build/image/ovmf-vars.fd`. Delete it to reset the firmware settings.
- To boot real hardware, write `basil.img` to a USB stick byte for byte (Rufus in DD mode, balenaEtcher, or `dd`) and turn off Secure Boot, since Limine is not signed. The image has one FAT32 EFI System Partition plus a hybrid MBR, so it boots on both UEFI and BIOS/CSM.

### Kernel tests

`make test-kernel` reports results through QEMU's `isa-debug-exit` device: exit status 33 (`(0x10 << 1) | 1`) means every test passed. `-no-reboot` turns a triple fault into a failure instead of a boot loop.

### WSL

`run-win` and `run-win-bios` use the Windows build of QEMU, which opens a native Windows window that snaps and tiles properly, unlike WSLg windows. It reads the build output through `\\wsl.localhost` paths.

- The Windows Hypervisor Platform cannot run OVMF, so UEFI runs under TCG and only BIOS boots get hardware acceleration.
- whpx's APIC drops the 8259 PIC's interrupts, so `run-win-bios` uses `kernel-irqchip=off` to make QEMU emulate the interrupt controllers.
- `LANG=en_US` is passed through `WSLENV` so QEMU's GTK UI is in English.

## Editor setup

Use the flake's clangd wrapper (`nix run .#clangd`). It passes `--query-driver` so clangd reads GCC trunk's headers instead of nixpkgs' default libstdc++. The Zed config in `.zed/` already does this, and its tasks run `make run-win` and `make run-win-bios` inside the dev shell.

Cabin overwrites `compile_commands.json` with only the last command's targets, so the Makefile saves each command's database and merges them into `build/<profile>/compile_commands.json`. `.clangd` points at `build/dev` and removes GCC-only flags (`-freflection`, `-mno-80387`) that clang rejects.

## Build notes

- The kernel's compiler flags live in `kernel/cabin.toml` under `[target.'cfg(os = "linux")'.profile]`. The cfg is always true on the build host and keeps the freestanding flags off libk's host tests. They apply only to C++, so a `.c` file would also need them in `cflags`.
- The linker script path in `kernel/cabin.toml` is relative to `build/<profile>/`, where ninja runs. Cabin does not track `kernel/linker.ld`, so the Makefile deletes the kernel when the script is newer to force a relink.
- `limine.h` comes from the `limine-protocol` flake input through `CPPFLAGS`, because Cabin's `include-dirs` must stay inside the package.
- The Makefile adds `-fmacro-prefix-map` so `source_location` paths in kernel logs are relative to the repository root.
- GoogleTest is built with GCC trunk and linked statically, so it uses the same libstdc++ as the tests.
- The dev shell disables Nix's hardening flags, which would add a stack protector and PIE to kernel code.
