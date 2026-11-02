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
    boot-logo-src = {
      url = "github:qemus/boot-logo";
      flake = false;
    };
    limine-protocol = {
      url = "github:limine-bootloader/limine-protocol";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, gcc-nightly, cabin-src, boot-logo-src, limine-protocol }:
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

      # Swaps the TianoCore splash inside an OVMF image (see LOGO in the Makefile).
      boot-logo = pkgs.buildGoModule {
        pname = "boot-logo";
        version = boot-logo-src.shortRev or "dirty";
        src = boot-logo-src;
        modRoot = "src";
        vendorHash = "sha256-P/8XdL+uBRCXLpF4ygfv/Z6MKsinXWVZk6TzgSK6D1U=";
        env.CGO_ENABLED = 0;
        ldflags = [ "-s" "-w" "-X main.Version=${boot-logo-src.shortRev or "dirty"}" ];
        meta.mainProgram = "boot-logo";
      };

      # The default build omits the El Torito images needed for a hybrid ISO.
      limine = pkgs.limine.override { buildCDs = true; };

      # clangd otherwise reads nixpkgs' default libstdc++ headers instead of
      # GCC trunk's; --query-driver lets it ask the real compiler.
      clangd = pkgs.writeShellScriptBin "clangd" ''
        exec ${pkgs.llvmPackages_22.clang-tools}/bin/clangd \
          --query-driver=${pkgs.gccTrunkStdenv.cc}/bin/g++ "$@"
      '';
    in
    {
      packages.${system} = {
        inherit cabin boot-logo;
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
            pkgs.jq # merges compile_commands.json (see Makefile)

            # Bootable images (GRUB/Limine ISOs, FAT EFI images)
            pkgs.grub2
            pkgs.xorriso
            pkgs.mtools
            pkgs.dosfstools
            limine
            pkgs.OVMF.fd
            boot-logo

            # Debugging / inspection
            clangd # before clang-tools so it shadows the unwrapped clangd
            pkgs.gdb
            pkgs.llvmPackages_22.clang-tools # clang-format, clang-tidy, clangd for cabin fmt/tidy

            # Every qemu-system-* plus qemu-img. Uses KVM when /dev/kvm is usable
            # (Linux, or WSL2 with WSLg for the window), else falls back to TCG.
            pkgs.qemu
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
