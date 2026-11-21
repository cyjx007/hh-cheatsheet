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
#   hh --scan          扫描 $PATH，列出本机所有命令（✓ = 已收录）
#   hh --scan -m       只列还没收录进速查表的命令
#   hh --scan -w [文件] 把未收录命令写成 tsv（默认 ~/.local/share/hh/commands.scan.tsv）
#   hh --add <命令…>   把命令追加进速查表（-f 文件，-c 分类）
#   hh --from-history  从 bash 历史里找高频且未收录的命令
#   hh -h / -V         帮助 / 版本

set -uo pipefail

VERSION="0.3.0"
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
  C_CAT=$'\033[36m' C_CMD=$'\033[32m' C_DIM=$'\033[2m' C_OK=$'\033[32m' C_RST=$'\033[0m'
else
  C_CAT='' C_CMD='' C_DIM='' C_OK='' C_RST=''
fi

# ---- 本机命令发现（--scan / --add / --from-history）----

# 去掉包名末尾的版本号/输出后缀：util-linux-2.42.4-bin → util-linux
pkg_short() {
  local p=$1
  p=${p%-bin}
  p=${p%-dev}
  p=${p%-man}
  p=${p%-doc}
  p=${p%-lib}
  p=${p%-out}
  p=${p%-info}
  p=${p%-terminfo}
  while [[ $p =~ -[0-9][^-]*$ ]]; do
    p=${p%"${BASH_REMATCH[0]}"}
  done
  printf '%s' "$p"
}

# $PATH 里的可执行命令：命令<TAB>包名<TAB>路径（同名只保留最靠前的一个）
path_commands() {
  local dir cmd path target pkg
  local -a dirs
  IFS=: read -r -a dirs <<<"$PATH"
  for dir in "${dirs[@]}"; do
    [ -n "$dir" ] && [ -d "$dir" ] || continue
    while IFS=$'\t' read -r cmd path target; do
      [ -x "$path" ] || continue
      case "$cmd" in .* | _* | '[') continue ;; esac
      if [ -n "$target" ] && [ "${target#/}" = "$target" ]; then
        target=$dir/$target
      fi
      case "${target:-$path}" in
        /nix/store/*)
          pkg=${target:-$path}
          pkg=${pkg#/nix/store/}
          pkg=${pkg%%/*}
          pkg=${pkg#*-}
          ;;
        /run/wrappers/*) pkg=wrappers ;;
        *) pkg=${dir##*/} ;;
      esac
      printf '%s\t%s\t%s\n' "$cmd" "$pkg" "$path"
    done < <(find "$dir" -maxdepth 1 \( -type f -o -type l \) -printf '%f\t%p\t%l\n' 2>/dev/null)
  done | awk -F'\t' '!seen[$1]++'
}

# --scan 依赖 GNU find 的 -printf（macOS 自带的是 BSD find）
require_gnu_find() {
  find / -maxdepth 0 -printf '' >/dev/null 2>&1 ||
    die "--scan 需要 GNU find（macOS 可装 findutils，用 gfind）"
}

# 速查表里已经出现过的命令名（每行只取命令字段的首个词，跳过 sudo/env 等前缀）
known_commands() {
  awk -F'\t' 'NF >= 2 { print $2 }' "$DATA" | while IFS= read -r line; do
    # shellcheck disable=SC2086
    set -- $line
    case "${1:-}" in
      sudo | command | env | time | nohup | exec) shift ;;
    esac
    [ $# -gt 0 ] && printf '%s\n' "$1"
  done
}

scan_commands() { # $1 = 1 时只显示未收录
  require_gnu_find
  local only_missing=${1:-0} mark cmd pkg path
  local -A known=()
  while IFS= read -r k; do known[$k]=1; done < <(known_commands)
  path_commands | while IFS=$'\t' read -r cmd pkg path; do
    if [ -n "${known[$cmd]-}" ]; then
      [ "$only_missing" = 1 ] && continue
      mark="${C_OK}✓${C_RST}"
    else
      mark=' '
    fi
    printf '%s\t%s\t%s%s%s\n' "$mark" "$cmd" "$C_DIM" "$(pkg_short "$pkg")" "$C_RST"
  done | sort -t$'\t' -k2,2 | awk -F'\t' '{ printf "%s  %-28s %s\n", $1, $2, $3 }'
}

write_scan() { # $1 = 输出文件：未收录命令写成 tsv，按包分组
  require_gnu_find
  local out=$1 tmp
  local -A known=()
  while IFS= read -r k; do known[$k]=1; done < <(known_commands)
  tmp=$(mktemp) || return 1
  path_commands | while IFS=$'\t' read -r cmd pkg path; do
    [ -n "${known[$cmd]-}" ] && continue
    printf '%s\t%s\n' "$cmd" "$(pkg_short "$pkg")"
  done | sort -t$'\t' -k2,2 -k1,1 | awk -F'\t' '
    $2 != last { printf "%s[%s]\n", (last == "" ? "" : "\n"), $2; last = $2 }
    { print $1 "\t" $1 }
  ' >"$tmp" || { rm -f -- "$tmp"; return 1; }
  mkdir -p -- "$(dirname -- "$out")" 2>/dev/null || true
  mv -- "$tmp" "$out" || { rm -f -- "$tmp"; return 1; }
  printf '未收录的命令已写入 %s（%s 条）\n' "$out" "$(grep -c $'\t' "$out")"
  printf '试用：%sHH_FILE=%s hh%s\n' "$C_DIM" "$out" "$C_RST"
}

# 从 man 页 NAME 段取一句话简介；没有 man 页就回退到包名
describe_cmd() { # $1 = 命令
  local cmd=$1 f line p
  f=$(man -w -- "$cmd" 2>/dev/null | head -1) || true
  if [ -n "$f" ] && [ -r "$f" ]; then
    case "$f" in
      *.gz) line=$(gzip -dc -- "$f" 2>/dev/null | awk '/^\.SH NAME/{getline; print; exit}') ;;
      *) line=$(awk '/^\.SH NAME/{getline; print; exit}' "$f" 2>/dev/null) ;;
    esac
    line=$(printf '%s' "$line" |
      sed -e 's/\\-/-/g' -e 's/\\f[BIRP]//g' -e 's/^[[:space:].]*//' -e 's/[[:space:]]*$//')
    case "$line" in *" - "*) line=${line#* - } ;; esac
    if [ -n "$line" ] && [ "$line" != "$cmd" ]; then
      printf '%s' "$line"
      return 0
    fi
  fi
  p=$(command -v -- "$cmd" 2>/dev/null) || p=''
  [ -n "$p" ] && p=$(readlink -f -- "$p" 2>/dev/null)
  case "$p" in
    /nix/store/*)
      p=${p#/nix/store/}
      p=${p%%/*}
      p=${p#*-}
      printf '来自 %s 包' "$(pkg_short "$p")"
      ;;
    '') printf '（当前 PATH 里没找到这个命令）' ;;
    *) printf '本机已安装命令' ;;
  esac
}

# 追加一条到 tsv 的指定分类（分类不存在就新建在末尾）
add_entry() { # $1=文件 $2=分类 $3=说明 $4=命令
  local file=$1 cat=$2 desc=$3 cmd=$4 tmp
  [ -w "$file" ] || return 1
  tmp=$(mktemp) || return 1
  awk -F'\t' -v cat="$cat" -v desc="$desc" -v cmd="$cmd" '
    function flush(   i, k) {
      k = n
      while (k > 0 && buf[k] == "") k--
      for (i = 1; i <= k; i++) print buf[i]
      print entry
      for (i = k + 1; i <= n; i++) print buf[i]
      n = 0
    }
    BEGIN { entry = desc "\t" cmd; inseg = 0; found = 0 }
    $0 == "[" cat "]" { inseg = 1; found = 1; print; next }
    inseg && /^\[/ { flush(); inseg = 0; print; next }
    {
      if (inseg) buf[++n] = $0
      else print
    }
    END {
      if (inseg) flush()
      if (!found) { print ""; print "[" cat "]"; print entry }
    }
  ' "$file" >"$tmp" || { rm -f -- "$tmp"; return 1; }
  mv -- "$tmp" "$file"
}

has_entry() { # $1=文件 $2=命令
  awk -F'\t' -v cmd="$2" '
    NF >= 2 { n = split($2, w, /[[:space:]]+/); for (i = 1; i <= n; i++) if (w[i] == cmd) found = 1 }
    END { exit(found ? 0 : 1) }
  ' "$1"
}

# --add 的默认目标：$HH_FILE，其次是可写的 ~/.config/hh/commands.tsv
default_add_file() {
  local f=${HH_FILE:-}
  if [ -n "$f" ]; then
    printf '%s' "$f"
    return 0
  fi
  f=$HOME/.config/hh/commands.tsv
  if [ -w "$f" ]; then
    printf '%s' "$f"
    return 0
  fi
  return 1
}

add_cmd_mode() { # 参数：-f 文件 -c 分类 命令…
  local file cat='本机命令' cmd desc added=0
  local -a cmds=()
  file=$(default_add_file) || file=''
  while [ $# -gt 0 ]; do
    case "$1" in
      -f | --file)
        [ $# -ge 2 ] || die "-f 需要文件路径"
        file=$2
        shift
        ;;
      -c | --category)
        [ $# -ge 2 ] || die "-c 需要分类名"
        cat=$2
        shift
        ;;
      -*) die "未知参数：$1（用 hh -h）" ;;
      *) cmds+=("$1") ;;
    esac
    shift
  done
  [ ${#cmds[@]} -gt 0 ] || die "用 hh --add <命令…>（可先用 hh --scan 看有什么）"
  [ -n "$file" ] || die "找不到可写的速查表；用 -f 指定（home-manager 管的目标文件需指向源文件）"
  [ -e "$file" ] || : >"$file" || die "无法创建 $file"
  for cmd in "${cmds[@]}"; do
    if has_entry "$file" "$cmd"; then
      printf '  %s已收录，跳过：%s%s\n' "$C_DIM" "$cmd" "$C_RST"
    else
      desc=$(describe_cmd "$cmd")
      add_entry "$file" "$cat" "$desc" "$cmd" || die "写入失败：$file"
      printf '  %s+ %s%s\n' "$C_OK" "$cmd" "$C_RST"
      printf '      %s\n' "$desc"
      added=$((added + 1))
    fi
  done
  [ "$added" -gt 0 ] && printf '\n已写入 %s 的 [%s]\n' "$file" "$cat"
  return 0
}

from_history_mode() { # 参数：-n N -f 文件（给 -f 时直接追加）
  local n=20 file='' cmd desc found=0 hist=${HISTFILE:-$HOME/.bash_history}
  while [ $# -gt 0 ]; do
    case "$1" in
      -n | --number)
        [ $# -ge 2 ] || die "-n 需要数字"
        n=$2
        shift
        ;;
      -f | --file)
        [ $# -ge 2 ] || die "-f 需要文件路径"
        file=$2
        shift
        ;;
      -*) die "未知参数：$1（用 hh -h）" ;;
      *) die "多余参数：$1" ;;
    esac
    shift
  done
  [ -r "$hist" ] || die "读不到历史文件：$hist"
  if [ -n "$file" ]; then
    [ -w "$file" ] || die "不可写：$file（可能由 home-manager 管理，用 -f 指向源文件）"
    [ -e "$file" ] || : >"$file" || die "无法创建 $file"
  fi
  local -A known=()
  while IFS= read -r k; do known[$k]=1; done < <(known_commands)
  printf '从 %s 统计的高频命令（跳过已收录和 shell 内建）：\n\n' "$hist"
  while IFS= read -r cmd; do
    case "$cmd" in
      cd | exit | pwd | clear | hh | sudo | command | env) continue ;;
    esac
    [ -n "${known[$cmd]-}" ] && continue
    desc=$(describe_cmd "$cmd")
    printf '  %-16s %s%s%s\n' "$cmd" "$C_DIM" "$desc" "$C_RST"
    if [ -n "$file" ]; then
      has_entry "$file" "$cmd" || add_entry "$file" '本机命令' "$desc" "$cmd" ||
        die "写入失败：$file"
    fi
    found=$((found + 1))
    [ "$found" -ge "$n" ] && break
  done < <(
    awk '{print $1}' "$hist" | grep -Ex '[[:alnum:]][[:alnum:]_.+-]*' |
      sort | uniq -c | sort -rn | awk '{print $2}'
  )
  [ "$found" -eq 0 ] && printf '  （没有新的了，表已经很全）\n'
  [ -n "$file" ] && printf '\n已追加 %d 条到 %s\n' "$found" "$file"
  return 0
}

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
      sed -n '2,/^$/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    --scan)
      shift
      scan_missing=0
      scan_out=''
      while [ $# -gt 0 ]; do
        case "$1" in
          -m | --missing) scan_missing=1 ;;
          -w | --write)
            if [ $# -ge 2 ] && [ "${2#-}" = "$2" ]; then
              scan_out=$2
              shift
            else
              scan_out=$HOME/.local/share/hh/commands.scan.tsv
            fi
            ;;
          *) die "未知参数：$1（用 hh -h）" ;;
        esac
        shift
      done
      if [ -n "$scan_out" ]; then write_scan "$scan_out"; else scan_commands "$scan_missing"; fi
      exit 0
      ;;
    --add)
      shift
      add_cmd_mode "$@"
      exit 0
      ;;
    --from-history)
      shift
      from_history_mode "$@"
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
      [ -n "$(entries "$2")" ] || die "没有这个分类：$2（用 hh -l 看）"
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
