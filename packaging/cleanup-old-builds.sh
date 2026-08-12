#!/usr/bin/env bash
# ============================================================
# 打包产物自动清理 — flare-client（三平台通用，各机器独立运行）
#
# 【触发方式】每次打包后立即调用本脚本（build-*.sh -p 已内置）
#   对每类包（rpm/dmg/exe/AppImage）只保留最新 N 个版本（默认 3），
#   超出的旧包立即删除。第 N+1 个包出现时自然触发删除，无需计数。
#
# 【双端独立】fedora/macmini 各自运行本脚本，互不影响；
#   各自保留自己机器上的打包产物，不用考虑另一台机器。
#
# 用法：
#   打包后调用:   ./cleanup-old-builds.sh
#   自定义数量:   ./cleanup-old-builds.sh 5
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/dist"

KEEP="${1:-3}"   # 保留最新 N 个版本（默认 3）

if [ ! -d "$DIST_DIR" ]; then
  echo "[cleanup] 无打包目录 $DIST_DIR，跳过"
  exit 0
fi

# 支持的文件类型（每类独立保留 KEEP 个）
PATTERNS=("flare-client_*.rpm" "flare-client_*.dmg" "flare-client_*.exe" "flare-client_*.AppImage" "flare-client_*.bin" "flare-client_*.macos")

TOTAL_DELETED=0
for pattern in "${PATTERNS[@]}"; do
  # 按修改时间倒序（最新在前）
  mapfile -t files < <(find "$DIST_DIR" -maxdepth 1 -name "$pattern" -type f -printf '%T@ %p\n' 2>/dev/null | sort -rn | cut -d' ' -f2-)
  count="${#files[@]}"
  if [ "$count" -le "$KEEP" ]; then
    continue
  fi
  # 删除超出的旧包（从最旧开始）
  for ((i=KEEP; i<count; i++)); do
    file="${files[$i]}"
    size=$(du -h "$file" 2>/dev/null | cut -f1)
    rm -f "$file"
    echo "[cleanup] 删除旧版本: $(basename "$file") (${size})"
    TOTAL_DELETED=$((TOTAL_DELETED + 1))
  done
done

if [ "$TOTAL_DELETED" -gt 0 ]; then
  echo "[cleanup] 完成：删除 $TOTAL_DELETED 个旧包（每类保留最新 $KEEP 个）"
else
  echo "[cleanup] 无需清理（每类保留最新 $KEEP 个）"
fi
exit 0
