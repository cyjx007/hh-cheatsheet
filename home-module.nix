{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.programs.hh;
in
{
  options.programs.hh = {
    enable = lib.mkEnableOption "hh, a two-level fzf command cheatsheet";

    package = lib.mkOption {
      type = lib.types.package;
      default = pkgs.callPackage ./package.nix { };
      defaultText = lib.literalExpression "pkgs.callPackage ./package.nix { }";
      description = "The hh package to install.";
    };

    cheatsheet = lib.mkOption {
      type = lib.types.nullOr lib.types.path;
      default = null;
      example = lib.literalExpression "./hh-commands.tsv";
      description = ''
        Your own cheatsheet, installed as ~/.config/hh/commands.tsv.
        When null, hh falls back to the bundled example.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    home.packages = [ cfg.package ];

    home.file.".config/hh/commands.tsv" = lib.mkIf (cfg.cheatsheet != null) {
      source = cfg.cheatsheet;
    };
  };
}
