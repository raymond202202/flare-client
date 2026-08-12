#!/usr/bin/env bash
# ============================================================
# macOS 构建脚本 — flare-client（macmini-hermes 执行）
# 依赖: Xcode CLT + Homebrew qt@6
#   xcode-select --install
#   brew install qt@6
# ============================================================
set -euo pipefail
cd "$(dirname "$0")/.."

# Homebrew Qt 路径（SSH 非交互 shell 不含此路径）
export PATH="/opt/homebrew/bin:$PATH"

if ! command -v qmake6 >/dev/null 2>&1; then
  echo "ERROR: qmake6 不存在。请先: brew install qt@6"
  exit 1
fi

echo "==> 配置 CMake (Release)"
cmake -B build-macos -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"

echo "==> 构建"
cmake --build build-macos -j"$(sysctl -n hw.ncpu)"

echo "==> 运行测试 (TDD)"
ctest --test-dir build-macos --output-on-failure

# 打包（可选：-p 参数触发 DMG 打包）
if [ "${1:-}" = "-p" ]; then
  echo "==> 打包 DMG"
  mkdir -p packaging/dist
  cp build-macos/flare-client.app/Contents/MacOS/flare-client \
     "packaging/dist/flare-client_$(grep -m1 'VERSION' CMakeLists.txt | sed 's/[^0-9.]*//g').macos"
  echo "==> 轮次清理（每构建 5 轮清理旧包，保留最新 3 个）"
  ./packaging/cleanup-old-builds.sh
fi

echo "==> 产物: build-macos/flare-client.app"
ls -d build-macos/flare-client.app 2>/dev/null || ls -lh build-macos/flare-client
