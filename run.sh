#!/usr/bin/env bash
# ============================================================
# Flare 客户端 一键启动（用户验收用）
# 用法: ./run.sh
# 或: bash run.sh
# ============================================================
set -e
cd "$(dirname "$0")"

BIN="./build-fedora/flare-client"

# 1) 如果没构建过，先构建
if [ ! -f "$BIN" ]; then
  echo "首次运行：正在构建 Flare 客户端…"
  if ! command -v cmake >/dev/null; then
    echo "❌ 缺少 cmake，请先运行: sudo dnf install cmake qt6-qtbase-devel gcc-c++"
    exit 1
  fi
  cmake -B build-fedora -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build build-fedora -j"$(nproc)" >/dev/null
fi

# 2) 检查 flare 引擎
if ! command -v flare >/dev/null 2>&1 && [ ! -x "$HOME/.flare/install/dist/cli/index.js" ]; then
  echo "⚠️  未检测到 flare 引擎，客户端窗口可打开但无法对话。"
fi

# 3) 启动（后台，避免终端阻塞）
echo "🚀 启动 Flare 客户端…"
"$BIN" &
disown
echo "窗口已打开。关闭窗口即可退出。"
