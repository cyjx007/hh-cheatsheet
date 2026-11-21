# hh — a two-level command cheatsheet

`hh` opens a small menu in your terminal: pick a category, pick a command, press Enter — the
command is copied to your clipboard.

It is for **your own** commands: the three flags you keep forgetting, the exact
`nixos-rebuild` incantation, the path to that router script. Two levels keep the list short
enough to scan; there is no tag syntax or query language to memorise.

```
┌ category> ─────────────────────────────┐
│ ▸ 下载 aria2c                          │
│   下载 yt-dlp                          │
│   播放 mpv                             │
│   系统更新 NixOS                        │
└────────────────────────────────────────┘
        ↓ Enter
┌ 命令> ─────────────────────────────────┐
│   多线程下载单个文件                     │
│ ▸ 断点续传 / 批量下载                    │
│   磁力 / BT                            │
└────────────────────────────────────────┘
        ↓ Enter → `aria2c -c -i urls.txt` is now in your clipboard
```

## Install

**Nix (flakes)**

```bash
nix run github:cyjx007/hh-cheatsheet          # try it
nix profile install github:cyjx007/hh-cheatsheet
```

**home-manager**

```nix
{
  inputs.hh-cheatsheet.url = "github:cyjx007/hh-cheatsheet";

  # in your home-manager module
  imports = [ inputs.hh-cheatsheet.homeModules.default ];
  programs.hh = {
    enable = true;
    cheatsheet = ./commands.tsv;   # your own file, installed as ~/.config/hh/commands.tsv
  };
}
```

**Anything else**

Copy `hh` somewhere in `$PATH` and install `fzf`. For clipboard support you also want
`wl-copy` (Wayland) or `xclip` (X11) — without them `hh` just prints the command.

## Cheatsheet format

Plain text, two kinds of lines:

```
[download]
Multi-threaded download	aria2c -x16 -s16 URL
Torrent or magnet	aria2c 'magnet:?xt=urn:btih:...'
Where the config lives	~/.config/aria2/aria2.conf
```

- `[name]` starts a category
- `description<TAB>command` is one entry (the command may be empty — useful as a note)

Write it in any language you like; the UI strings follow `LANG` (English / Chinese for now).

Search order: `$HH_FILE` → `$XDG_CONFIG_HOME/hh/commands.tsv` → `~/.config/hh/commands.tsv`
→ `~/.local/share/hh/commands.tsv` → the bundled example.

Start from `examples/commands.tsv` (English) or `examples/commands.zh.tsv` (Chinese).

## Usage

```bash
hh                  # two-level menu
hh aria2            # global search over every entry
hh -l               # print everything, grouped (description + indented command)
hh -lc              # print everything, one line per entry (grep friendly)
hh -c download      # print one category
hh -p               # print the selection instead of copying it
hh -V               # version
```

| Key | Action |
| --- | --- |
| `↑` `↓` | move |
| `Enter` | copy the command (or print it with `-p`) |
| `Esc` | back to the category list; at the top level, quit |
| `Ctrl-C` | quit |

## Discovering commands on your machine

`hh` can inspect what is actually installed and help fill the sheet:

```bash
hh --scan              # every executable in $PATH (✓ = already in your sheet)
hh --scan -m           # only commands not in the sheet yet
hh --scan -w           # write the missing ones to ~/.local/share/hh/commands.scan.tsv
hh --add htop          # append one command, description from its man page
hh --from-history      # frequent bash-history commands not in the sheet yet
```

`--scan` finds commands through `$PATH` and groups them by the package that provides them
(Nix store paths). `--add` / `--from-history -f FILE` append to `$HH_FILE`, or to a writable
`~/.config/hh/commands.tsv`. When home-manager manages that path as a read-only symlink,
point `-f` at the source file:

```bash
hh --add htop -f ~/nixos-config/home/hh-commands.tsv -c 系统监控
```

`--scan -w` writes a **separate** file instead of touching your sheet, so you can review it
first:

```bash
HH_FILE=~/.local/share/hh/commands.scan.tsv hh
```

`--scan` uses GNU find (`-printf`), so it is Linux-only; on macOS install `findutils` and use
`gfind`. `--add` / `--from-history` work anywhere.

## Why not navi / tldr?

| | `hh` | `navi` | `tldr` |
| --- | --- | --- | --- |
| Structure | fixed two levels | tags + fuzzy | one command |
| Content | yours | yours + `--cheatsh` | community |
| Written in | bash | Rust | Rust |
| Deps | `fzf` | `fzf` | — |

`tldr` answers *how does this tool work*. `navi` is great when you like tags and fuzzy
matching over a large collection. `hh` is deliberately dumber: a short, fixed menu of the
commands **you** actually use on **this** machine.

## Development

```bash
shellcheck hh      # CI runs this
nix build .#       # builds the package
nix run .#         # runs from the working tree
```

## License

MIT — see [LICENSE](LICENSE).
