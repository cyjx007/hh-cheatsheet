{
  description = "hh — a two-level, fzf-powered command cheatsheet";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems =
        f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (
        pkgs:
        let
          hh = pkgs.callPackage ./package.nix { };
        in
        {
          inherit hh;
          default = hh;
        }
      );

      overlays.default = final: prev: {
        hh = final.callPackage ./package.nix { };
      };

      # home-manager: programs.hh.enable = true; programs.hh.cheatsheet = ./commands.tsv;
      homeModules.default = ./home-module.nix;

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
