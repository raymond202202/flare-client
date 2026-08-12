#!/usr/bin/env bash
# ============================================================
# 打包产物自动清理 — flare-client（三平台通用）
# 策略：packaging/dist/ 下保留最新 N 个版本，旧的自动删除
# 默认保留 3 个；可用参数覆盖：./cleanup-old-builds.sh 5
#
# 用法：
#   手动清理:   ./cleanup-old-builds.sh [保留数量]
#   打包后自动: 构建脚本末尾调用本脚本
#   定时兜底:   cron 每日执行本脚本
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/dist"
KEEP="${1:-3}"   # 默认保留 3 个版本

if [ ! -d "$DIST_DIR" ]; then
  echo "[cleanup] 无打包目录 $DIST_DIR，跳过"
  exit 0
fi

# 支持的包文件类型（按版本命名：flare-client_<ver>.<ext>）
# 例: flare-client_0.1.0-1.fc44.x86_64.rpm
#     flare-client_0.2.0-universal.dmg
#     flare-client_0.3.0-x64.exe
PATTERNS=("flare-client_*.rpm" "flare-client_*.dmg" "flare-client_*.exe" "flare-client_*.AppImage")

TOTAL_DELETED=0
TOTAL_FREED=0

for pattern in "${PATTERNS[@]}"; do
  # 按修改时间倒序排列（最新在前）
  mapfile -t files < <(find "$DIST_DIR" -maxdepth 1 -name "$pattern" -type f -printf '%T@ %p\n' 2>/dev/null | sort -rn | cut -d' ' -f2-)
  count="${#files[@]}"

  if [ "$count" -le "$KEEP" ]; then
    echo "[cleanup] $pattern: 仅 $count 个（<=$KEEP），无需清理"
    continue
  fi

  # 删除保留数量之后的旧版本
  for ((i=KEEP; i<count; i++)); do
    file="${files[$i]}"
    size=$(du -h "$file" | cut -f1)
    rm -f "$file"
    echo "[cleanup] 删除旧版本: $file (${size})"
    TOTAL_DELETED=$((TOTAL_DELETED + 1))
  done
done

echo "[cleanup] 完成：删除 $TOTAL_DELETED 个旧包"
exit 0
