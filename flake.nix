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
      clang-tools = pkgs.llvmPackages_22.clang-tools;

      cabin = pkgs.rustPlatform.buildRustPackage {
        pname = "cabin";
        version = (lib.importTOML "${cabin-src}/Cargo.toml").workspace.package.version
          + "-" + (cabin-src.shortRev or "dirty");
        src = cabin-src;
        cargoLock.lockFile = "${cabin-src}/Cargo.lock";
        cargoBuildFlags = [ "-p" "cabinpkg" ];
        doCheck = false;
        meta.mainProgram = "cabin";
      };

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

      gtest = pkgs.gtest.override {
        stdenv = pkgs.gccTrunkStdenv;
        static = true;
      };

      limine = pkgs.limine.override { buildCDs = true; };

      clangd = pkgs.writeShellScriptBin "clangd" ''
        exec ${clang-tools}/bin/clangd --query-driver=${pkgs.gccTrunkStdenv.cc}/bin/g++ "$@"
      '';
    in
    {
      packages.${system} = {
        inherit cabin boot-logo clangd;
        inherit (pkgs) gcc-trunk;
        default = cabin;
      };

      devShells.${system}.default = pkgs.mkShell.override { stdenv = pkgs.gccTrunkStdenv; } {
        name = "basil";

        packages = with pkgs; [
          cabin
          fish
          starship

          binutils
          nasm
          gnumake
          ninja
          pkg-config
          jq

          grub2
          xorriso
          mtools
          gptfdisk
          dosfstools
          limine
          OVMF.fd
          boot-logo

          clangd
          gdb
          clang-tools

          qemu
        ];

        buildInputs = [ gtest ];

        hardeningDisable = [ "all" ];

        CPPFLAGS = "-I${limine-protocol}/include";
        LIMINE_DATA = "${limine}/share/limine";
        LIMINE_INCLUDE = "${limine-protocol}/include";
        OVMF_CODE = "${pkgs.OVMF.fd}/FV/OVMF_CODE.fd";
        OVMF_VARS = "${pkgs.OVMF.fd}/FV/OVMF_VARS.fd";

        shellHook = ''
          echo "basil dev shell: $($CXX --version | head -1)"
          echo "cabin $(cabin --version 2>/dev/null | head -1)"

          if [[ $- == *i* && -z "''${BASIL_NO_FISH:-}" ]]; then
            export name=basil
            export STARSHIP_CONFIG="''${BASIL_STARSHIP_CONFIG:-${./starship.toml}}"
            exec ${pkgs.fish}/bin/fish -C 'set -g fish_greeting; ${pkgs.starship}/bin/starship init fish | source'
          fi
        '';
      };
    };
}
