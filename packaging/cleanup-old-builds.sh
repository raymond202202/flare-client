#!/usr/bin/env bash
# ============================================================
# 打包产物自动清理 — flare-client（三平台通用，各机器独立运行）
#
# 【触发方式】构建轮次驱动，不是日历时间！
#   每次 build-*.sh -p 打包时调用本脚本，内部计数：
#     - 每构建 CLEAN_EVERY 轮（默认 5 轮）清理一次旧包
#     - dist 目录超 MAX_KEEP（默认 3 个）版本时也会立即清理（双保险）
#
# 【双端独立】fedora/macmini 各自维护自己的计数文件
#   （~/.flare-client-build-count），互不影响、不用考虑另一台机器。
#
# 用法：
#   打包后调用:   ./cleanup-old-builds.sh
#   手动强制:     ./cleanup-old-builds.sh --force
#   自定义轮次:   ./cleanup-old-builds.sh --every 3 --keep 2
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/dist"
COUNT_FILE="$HOME/.flare-client-build-count"

KEEP="${KEEP:-3}"          # 保留最新 N 个版本
EVERY="${EVERY:-5}"        # 每构建 N 轮清理一次
FORCE=0

# 简单参数解析
while [ $# -gt 0 ]; do
  case "$1" in
    --force) FORCE=1 ;;
    --keep) KEEP="$2"; shift ;;
    --every) EVERY="$2"; shift ;;
    *) echo "未知参数: $1" >&2; exit 1 ;;
  esac
  shift
done

if [ ! -d "$DIST_DIR" ]; then
  echo "[cleanup] 无打包目录 $DIST_DIR，跳过（计数+1 继续）"
  # 无产物也要计数，保持节奏
  [ "$FORCE" = "1" ] || echo "0" > "$COUNT_FILE"
  exit 0
fi

# 计数逻辑：每构建 +1，到 EVERY 轮就清理
COUNT=0
if [ -f "$COUNT_FILE" ]; then
  COUNT=$(cat "$COUNT_FILE" 2>/dev/null || echo 0)
fi
COUNT=$((COUNT + 1))

SHOULD_CLEAN=0
if [ "$FORCE" = "1" ]; then
  SHOULD_CLEAN=1
  echo "[cleanup] 手动强制清理"
elif [ "$COUNT" -ge "$EVERY" ]; then
  SHOULD_CLEAN=1
  echo "[cleanup] 已构建 $COUNT 轮，达到清理阈值（每 $EVERY 轮）"
fi

# 支持的文件类型
PATTERNS=("flare-client_*.rpm" "flare-client_*.dmg" "flare-client_*.exe" "flare-client_*.AppImage" "flare-client_*.bin" "flare-client_*.macos")

if [ "$SHOULD_CLEAN" = "1" ]; then
  TOTAL_DELETED=0
  for pattern in "${PATTERNS[@]}"; do
    mapfile -t files < <(find "$DIST_DIR" -maxdepth 1 -name "$pattern" -type f -printf '%T@ %p\n' 2>/dev/null | sort -rn | cut -d' ' -f2-)
    count="${#files[@]}"
    if [ "$count" -le "$KEEP" ]; then
      continue
    fi
    for ((i=KEEP; i<count; i++)); do
      file="${files[$i]}"
      size=$(du -h "$file" 2>/dev/null | cut -f1)
      rm -f "$file"
      echo "[cleanup] 删除旧版本: $(basename "$file") (${size})"
      TOTAL_DELETED=$((TOTAL_DELETED + 1))
    done
  done
  echo "[cleanup] 完成：删除 $TOTAL_DELETED 个旧包"
  COUNT=0   # 清理后重置计数
else
  echo "[cleanup] 距下次清理还有 $((EVERY - COUNT)) 轮（保留最新 $KEEP 版）"
fi

# 双保险：即使没到轮次，超过 KEEP*2 也立即清理（防极端情况）
for pattern in "${PATTERNS[@]}"; do
  mapfile -t files < <(find "$DIST_DIR" -maxdepth 1 -name "$pattern" -type f -printf '%T@ %p\n' 2>/dev/null | sort -rn | cut -d' ' -f2-)
  if [ "${#files[@]}" -gt $((KEEP * 2)) ]; then
    echo "[cleanup] 产物超双倍阈值($((KEEP*2)))，立即清理"
    for ((i=KEEP; i<${#files[@]}; i++)); do
      rm -f "${files[$i]}"
    done
    echo "0" > "$COUNT_FILE"
  fi
done

# 保存计数
echo "$COUNT" > "$COUNT_FILE"
exit 0
