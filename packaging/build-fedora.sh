#!/usr/bin/env bash
# ============================================================
# Fedora 构建脚本 — flare-client
# 依赖: g++ / cmake / qt6-qtbase-devel
#   sudo dnf install gcc-c++ cmake qt6-qtbase-devel
# ============================================================
set -euo pipefail
cd "$(dirname "$0")/.."

echo "==> 配置 CMake (Release)"
cmake -B build-fedora -DCMAKE_BUILD_TYPE=Release

echo "==> 构建"
cmake --build build-fedora -j"$(nproc)"

echo "==> 运行测试 (TDD)"
ctest --test-dir build-fedora --output-on-failure

# 打包（可选：-p 参数触发 RPM 打包 + 自动清理旧包）
if [ "${1:-}" = "-p" ]; then
  echo "==> 打包 RPM"
  # 生成 .spec 后在此打包；产物输出到 packaging/dist/
  mkdir -p packaging/dist
  cp build-fedora/flare-client packaging/dist/flare-client_$(grep -m1 "VERSION" CMakeLists.txt | sed 's/[^0-9.]*//g').bin
  echo "==> 清理旧包（保留最新 3 个）"
  ./packaging/cleanup-old-builds.sh 3
fi

echo "==> 产物: build-fedora/flare-client"
ls -lh build-fedora/flare-client
