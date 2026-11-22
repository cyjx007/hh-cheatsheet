{
  lib,
  stdenv,
  makeWrapper,
  fzf,
  wl-clipboard,
  xclip,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "hh";
  version = "0.3.2";

  src = ./.;

  nativeBuildInputs = [ makeWrapper ];

  installPhase = ''
    runHook preInstall
    install -Dm755 hh $out/bin/hh
    install -Dm644 examples/commands.tsv $out/share/hh/commands.tsv
    install -Dm644 examples/commands.zh.tsv $out/share/hh/commands.zh.tsv
    wrapProgram $out/bin/hh \
      --prefix PATH : ${
        lib.makeBinPath ([ fzf ] ++ lib.optionals stdenv.hostPlatform.isLinux [ wl-clipboard xclip ])
      }
    runHook postInstall
  '';

  meta = {
    description = "Two-level, fzf-powered command cheatsheet";
    homepage = "https://github.com/cyjx007/hh-cheatsheet";
    license = lib.licenses.mit;
    mainProgram = "hh";
    platforms = lib.platforms.unix;
  };
})
