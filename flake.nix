{
  description = "basil: OS development environment (GCC trunk + Cabin)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    gcc-nightly = {
      url = "github:Aliremu/gcc-nightly";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    cabin-src = {
      url = "github:Aliremu/cabin";
      flake = false;
    };
    limine-protocol = {
      url = "github:limine-bootloader/limine-protocol";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, gcc-nightly, cabin-src, limine-protocol }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        overlays = [ gcc-nightly.overlays.default ];
      };
      lib = pkgs.lib;

      cabin = pkgs.rustPlatform.buildRustPackage {
        pname = "cabin";
        version = (lib.importTOML "${cabin-src}/Cargo.toml").workspace.package.version
          + "-" + (cabin-src.shortRev or "dirty");
        src = cabin-src;
        cargoLock.lockFile = "${cabin-src}/Cargo.lock";
        cargoBuildFlags = [ "-p" "cabinpkg" ];
        # The test suite shells out to compilers and the registry.
        doCheck = false;
        meta.mainProgram = "cabin";
      };

      # The default build omits the El Torito images needed for a hybrid ISO.
      limine = pkgs.limine.override { buildCDs = true; };

      # QEMU lives on the Windows side; call it through WSL interop. Existing
      # Linux paths (bare, or as `key=path` inside comma-separated options)
      # are rewritten with wslpath so the Windows binary can open them;
      # relative paths must be converted too, as QEMU's block layer fails on
      # files resolved against a \\wsl.localhost working directory.
      qemuDir = "/mnt/c/Program Files/qemu";
      qemuExe = name: exe: pkgs.writeShellScriptBin name ''
        args=()
        for arg in "$@"; do
          IFS=, read -ra parts <<< "$arg"
          out=()
          for p in "''${parts[@]}"; do
            key=""; val="$p"
            if [[ "$p" == *=* ]]; then key="''${p%%=*}="; val="''${p#*=}"; fi
            if [[ -e "$val" && ( "$val" == */* || -f "$val" ) ]]; then
              val="$(wslpath -w "$(realpath "$val")")"
            fi
            out+=("$key$val")
          done
          args+=("$(IFS=,; echo "''${out[*]}")")
        done
        exec "${qemuDir}/${exe}" "''${args[@]}"
      '';
      qemuWrapper = arch: qemuExe "qemu-system-${arch}" "qemu-system-${arch}.exe";
      qemuImg = qemuExe "qemu-img" "qemu-img.exe";
    in
    {
      packages.${system} = {
        inherit cabin;
        inherit (pkgs) gcc-trunk;
        default = cabin;
      };

      devShells.${system}.default =
        pkgs.mkShell.override { stdenv = pkgs.gccTrunkStdenv; } {
          # Shown by the prompt's nix_shell segment.
          name = "basil";

          packages = [
            cabin
            pkgs.fish
            pkgs.starship

            # Build
            pkgs.binutils
            pkgs.nasm
            pkgs.gnumake
            pkgs.ninja
            pkgs.pkg-config

            # Bootable images (GRUB/Limine ISOs, FAT EFI images)
            pkgs.grub2
            pkgs.xorriso
            pkgs.mtools
            pkgs.dosfstools
            limine
            pkgs.OVMF.fd

            # Debugging / inspection
            pkgs.gdb
            pkgs.clang-tools # clang-format, clang-tidy, clangd for cabin fmt/tidy

            (qemuWrapper "x86_64")
            (qemuWrapper "i386")
            (qemuWrapper "aarch64")
            (qemuWrapper "riscv64")
            qemuImg
          ];

          # Kernel code must not get Nix's hardening flags (stack protector, PIE, ...).
          hardeningDisable = [ "all" ];

          LIMINE_DATA = "${limine}/share/limine";
          LIMINE_INCLUDE = "${limine-protocol}/include";
          # Cabin's include-dirs must stay inside the package, so limine.h
          # comes in through the conventional variable Cabin also reads.
          CPPFLAGS = "-I${limine-protocol}/include";
          OVMF_CODE = "${pkgs.OVMF.fd}/FV/OVMF_CODE.fd";
          OVMF_VARS = "${pkgs.OVMF.fd}/FV/OVMF_VARS.fd";

          shellHook = ''
            echo "basil dev shell: $($CXX --version | head -1)"
            echo "cabin $(cabin --version 2>/dev/null | head -1)"

            # Interactive `nix develop` drops into fish; `nix develop --command`
            # and BASIL_NO_FISH=1 keep bash.
            if [[ $- == *i* && -z "''${BASIL_NO_FISH:-}" ]]; then
              export name=basil # mkShell reports "basil-env"
              export STARSHIP_CONFIG="''${BASIL_STARSHIP_CONFIG:-${./starship.toml}}"
              exec ${pkgs.fish}/bin/fish -C 'set -g fish_greeting; ${pkgs.starship}/bin/starship init fish | source'
            fi
          '';
        };
    };
}
