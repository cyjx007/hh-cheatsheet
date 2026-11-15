# hh — 两级命令速查

`hh` 在终端里开一个小菜单：选分类 → 选命令 → 回车，命令就进了剪贴板。

它是给**你自己**的命令用的：老是记不住的那几个参数、`nixos-rebuild` 的完整写法、某个脚本的
路径。两级菜单让列表短到能一眼扫完，没有标签语法，也没有要背的查询语言。

```
分类>  下载 aria2c / 下载 yt-dlp / 播放 mpv / 系统更新 NixOS …
        ↓ 回车
命令>  多线程下载单个文件 / 断点续传 / 磁力 BT …
        ↓ 回车 → `aria2c -c -i urls.txt` 已在剪贴板
```

## 安装

**Nix（flake）**

```bash
nix run github:cyjx007/hh-cheatsheet          # 直接试
nix profile install github:cyjx007/hh-cheatsheet
```

**home-manager**

```nix
{
  inputs.hh-cheatsheet.url = "github:cyjx007/hh-cheatsheet";

  imports = [ inputs.hh-cheatsheet.homeModules.default ];
  programs.hh = {
    enable = true;
    cheatsheet = ./commands.tsv;   # 你自己的速查表 → ~/.config/hh/commands.tsv
  };
}
```

**其它系统**：把 `hh` 放进 `$PATH`，装好 `fzf`；想要剪贴板再加 `wl-copy`（Wayland）或
`xclip`（X11），没有的话 `hh` 只把命令打印出来。

## 速查表格式

纯文本，两种行：

```
[下载]
多线程下载单个文件	aria2c -x16 -s16 URL
磁力 / BT	aria2c 'magnet:?xt=urn:btih:...'
配置文件在哪	~/.config/aria2/aria2.conf
```

- `[名字]` 开始一个分类
- `说明<TAB>命令` 是一条（命令可以为空，当纯备注用）

内容写什么语言都行；界面提示跟着 `LANG` 走（目前英文 / 中文）。

查找顺序：`$HH_FILE` → `$XDG_CONFIG_HOME/hh/commands.tsv` → `~/.config/hh/commands.tsv`
→ `~/.local/share/hh/commands.tsv` → 自带的示例。

可以从 `examples/commands.tsv`（英文）或 `examples/commands.zh.tsv`（中文）抄起。

## 用法

```bash
hh                  # 两级菜单
hh aria2            # 全局搜索（不分层）
hh -l               # 列出全部（分组：说明一行，命令缩进一行）
hh -lc              # 列出全部（一行一条，给 grep 用）
hh -c 下载           # 只打印某个分类
hh -p               # 只打印选中的命令，不复制
hh -V               # 版本
```

| 键 | 作用 |
| --- | --- |
| `↑` `↓` | 上下移动 |
| `Enter` | 复制命令（`-p` 时改为打印） |
| `Esc` | 返回上一级分类；在分类层则退出 |
| `Ctrl-C` | 退出 |

## 和 navi / tldr 的区别

| | `hh` | `navi` | `tldr` |
| --- | --- | --- | --- |
| 结构 | 固定两级 | 标签 + 模糊匹配 | 单个命令 |
| 内容 | 你自己的 | 自己的 + `--cheatsh` | 社区 |
| 实现 | bash | Rust | Rust |
| 依赖 | `fzf` | `fzf` | — |

`tldr` 回答「这个工具怎么用」；`navi` 适合喜欢标签、条目很多的人；`hh` 故意做得更简单——
一个固定的短菜单，装的是**这台机器上你实际会敲**的命令。

## 开发

```bash
shellcheck hh      # CI 会跑
nix build .#       # 构建
nix run .#         # 直接从工作区运行
```

## 许可

MIT，见 [LICENSE](LICENSE)。
