#!/usr/bin/env bash
# hh — 两级命令速查（fzf 驱动）
#
# 数据格式（纯文本，一行一条）：
#   [分类名]
#   说明<TAB>命令
#
# 用法：
#   hh                 两级菜单：分类 → 命令
#   hh <关键词>        全局搜索（不分层，直接搜所有条目）
#   hh -l              列出全部（分组两行：说明 / 缩进的命令）
#   hh -lc             列出全部（一行一条，给 grep 用）
#   hh -c <分类>       只打印某个分类（fzf 预览用这个）
#   hh -p              只打印选中的命令，不复制
#   hh -h / -V         帮助 / 版本

set -uo pipefail

VERSION="0.2.1"
SELF_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

# ---- 语言（只看界面提示，数据内容原样显示）----
case "${LC_ALL:-${LC_MESSAGES:-${LANG:-}}}" in
  zh* | *_CN* | *_TW* | *_HK*) ZH=1 ;;
  *) ZH=0 ;;
esac
if [ "$ZH" = 1 ]; then
  T_CAT='分类> '; T_CMD='命令> '; T_SEARCH='搜索> '
  H_L1='↑↓ 选分类 · 回车进入 · Esc 退出'
  H_L2='↑↓ 选命令 · 回车复制 · Esc 返回分类'
  H_SEARCH='↑↓ 选命令 · 回车复制 · Esc 退出'
  M_COPIED='已复制到剪贴板'
  M_NOCLIP='没找到剪贴板工具（wl-copy / xclip / pbcopy），命令已打印在上面'
else
  T_CAT='category> '; T_CMD='command> '; T_SEARCH='search> '
  H_L1='↑↓ category · Enter open · Esc quit'
  H_L2='↑↓ command · Enter copy · Esc back'
  H_SEARCH='↑↓ command · Enter copy · Esc quit'
  M_COPIED='copied to clipboard'
  M_NOCLIP='no clipboard tool found (wl-copy / xclip / pbcopy); printed above'
fi

die() {
  printf 'hh: %s\n' "$*" >&2
  exit 1
}

# ---- 找数据文件：HH_FILE > XDG 配置 > 用户数据 > 仓库自带示例 ----
find_data() {
  local cand
  for cand in \
    "${HH_FILE:-}" \
    "${XDG_CONFIG_HOME:-$HOME/.config}/hh/commands.tsv" \
    "$HOME/.local/share/hh/commands.tsv" \
    "$SELF_DIR/examples/commands.tsv" \
    "$SELF_DIR/../share/hh/commands.tsv"; do
    [ -n "$cand" ] && [ -r "$cand" ] && {
      printf '%s' "$cand"
      return 0
    }
  done
  return 1
}

DATA=$(find_data) || die "找不到速查表（HH_FILE / ~/.config/hh/commands.tsv / ~/.local/share/hh/commands.tsv）"

categories() { awk '/^\[/{print substr($0, 2, length($0) - 2)}' "$DATA"; }
entries() { awk -F'\t' -v t="$1" '$0 == "[" t "]"{f=1; next} /^\[/{f=0} f && NF>=2{print}' "$DATA"; }
all_entries() { awk -F'\t' '/^\[/{c=substr($0,2,length($0)-2); next} NF>=2{printf "[%s] %s\t%s\n", c, $1, $2}' "$DATA"; }

# ---- 输出着色（只在终端里）----
if [ -t 1 ]; then
  C_CAT=$'\033[36m' C_CMD=$'\033[32m' C_DIM=$'\033[2m' C_RST=$'\033[0m'
else
  C_CAT='' C_CMD='' C_DIM='' C_RST=''
fi

# 每条两行：说明一行，命令缩进一行（长命令换行时也不会和说明糊在一起）
print_grouped() { # $1 = 分类，$2 = 1 时打印分类标题
  local cat=$1 header=${2:-0}
  [ "$header" = 1 ] && printf '\n%s▸ %s%s\n' "$C_CAT" "$cat" "$C_RST"
  entries "$cat" | while IFS=$'\t' read -r desc cmd; do
    printf '  %s\n' "$desc"
    [ -n "$cmd" ] && printf '      %s%s%s\n' "$C_CMD" "$cmd" "$C_RST"
  done
}

# 一行一条（给 grep 用）：[分类] 说明 → 命令
print_compact() { # $1 = 分类，$2 = 1 时带分类名
  local cat=$1 with_cat=${2:-0}
  entries "$cat" | while IFS=$'\t' read -r desc cmd; do
    if [ -n "$cmd" ]; then
      if [ "$with_cat" = 1 ]; then
        printf '%s[%s]%s %s %s→%s %s%s%s\n' "$C_CAT" "$cat" "$C_RST" "$desc" "$C_DIM" "$C_RST" "$C_CMD" "$cmd" "$C_RST"
      else
        printf '%s %s→%s %s%s%s\n' "$desc" "$C_DIM" "$C_RST" "$C_CMD" "$cmd" "$C_RST"
      fi
    else
      printf '%s[%s]%s %s\n' "$C_CAT" "$cat" "$C_RST" "$desc"
    fi
  done
}

copy_cmd() {
  local cmd=$1
  [ -n "$cmd" ] || return 1
  if command -v wl-copy >/dev/null 2>&1 && [ -n "${WAYLAND_DISPLAY:-}" ]; then
    printf '%s' "$cmd" | wl-copy 2>/dev/null && return 0
  fi
  if command -v xclip >/dev/null 2>&1 && [ -n "${DISPLAY:-}" ]; then
    printf '%s' "$cmd" | xclip -selection clipboard 2>/dev/null && return 0
  fi
  if command -v pbcopy >/dev/null 2>&1; then
    printf '%s' "$cmd" | pbcopy 2>/dev/null && return 0
  fi
  return 1
}

finish() { # $1 = 选中行
  local desc cmd
  desc=$(printf '%s' "$1" | cut -f1)
  cmd=$(printf '%s' "$1" | cut -f2-)
  printf '\n%s▸ %s%s\n' "$C_CAT" "$desc" "$C_RST"
  [ -n "$cmd" ] && printf '  %s%s%s\n' "$C_CMD" "$cmd" "$C_RST"
  if [ "$PRINT_ONLY" = 1 ]; then
    [ -n "$cmd" ] && printf '%s\n' "$cmd"
    return 0
  fi
  if [ -n "$cmd" ]; then
    if copy_cmd "$cmd"; then
      printf '  %s%s%s\n' "$C_DIM" "$M_COPIED" "$C_RST"
    else
      printf '  %s%s%s\n' "$C_DIM" "$M_NOCLIP" "$C_RST"
    fi
  fi
}

FZF_COMMON=(--height=60% --reverse --border --info=inline --pointer='▸ ')

PRINT_ONLY=0
query=''
while [ $# -gt 0 ]; do
  case "$1" in
    -h | --help)
      sed -n '2,16p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    -V | --version)
      printf 'hh %s\n' "$VERSION"
      exit 0
      ;;
    -l | --list)
      categories | while read -r c; do print_grouped "$c" 1; done
      exit 0
      ;;
    -lc | --list-compact)
      categories | while read -r c; do print_compact "$c" 1; done
      exit 0
      ;;
    -c | --category)
      [ $# -ge 2 ] || die "-c 需要分类名（先用 hh -l 看）"
      print_grouped "$2" 0
      exit 0
      ;;
    -p | --print) PRINT_ONLY=1 ;;
    -*) die "未知参数：$1（用 hh -h）" ;;
    *) query=$1 ;;
  esac
  shift
done

# ---- 全局搜索模式：hh <关键词> ----
if [ -n "$query" ]; then
  sel=$(all_entries | fzf "${FZF_COMMON[@]}" --delimiter='\t' --with-nth=1 \
    --prompt="$T_SEARCH" --header="$H_SEARCH" --query "$query" \
    --preview 'printf "%s\n" {} | cut -f2-' --preview-window=down:3:wrap) || exit 0
  [ -n "${sel-}" ] || exit 0
  finish "$sel"
  exit 0
fi

# ---- 两级菜单；二级 Esc 返回一级 ----
while :; do
  tool=$(categories | fzf "${FZF_COMMON[@]}" --prompt="$T_CAT" --header="$H_L1" \
    --preview "$(printf '%q' "$0") -c {}" --preview-window=down:45%:wrap) || exit 0
  [ -n "${tool-}" ] || exit 0

  while :; do
    sel=$(entries "$tool" | fzf "${FZF_COMMON[@]}" --delimiter='\t' --with-nth=1 \
      --prompt="$T_CMD" --header="$H_L2" \
      --preview 'printf "%s\n" {} | cut -f2-' --preview-window=down:3:wrap) || break
    [ -n "${sel-}" ] || break
    finish "$sel"
    exit 0
  done
done
