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

echo "==> 产物: build-fedora/flare-client"
ls -lh build-fedora/flare-client
