{
  inputs = {
    utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11"; # last version to support gcc-arm-embedded-10
  };
  outputs = { self, nixpkgs, utils }: utils.lib.eachDefaultSystem (system:
    let
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShell = pkgs.mkShell.override { stdenv = pkgs.clang16Stdenv; } {
        buildInputs = with pkgs; [
          gcc-arm-embedded-10
          libblocksruntime
          openssl
          python312Packages.compiledb
        ];
      };
    }
  );
}
