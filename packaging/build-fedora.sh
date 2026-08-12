#!/usr/bin/env bash
# ============================================================
# Fedora 构建脚本 — flare-client
# 依赖: g++ / cmake / qt6-qtbase-devel / rpm-build
#   sudo dnf install gcc-c++ cmake qt6-qtbase-devel rpm-build
#
# 用法:
#   ./packaging/build-fedora.sh          # 仅构建 + 测试
#   ./packaging/build-fedora.sh -p       # 构建 + 测试 + 打包 RPM + 自动清理
# ============================================================
set -euo pipefail
cd "$(dirname "$0")/.."

echo "==> 配置 CMake (Release)"
cmake -B build-fedora -DCMAKE_BUILD_TYPE=Release

echo "==> 构建"
cmake --build build-fedora -j"$(nproc)"

echo "==> 运行测试 (TDD)"
ctest --test-dir build-fedora --output-on-failure

# 打包（可选：-p 参数触发 CPack RPM 打包）
if [ "${1:-}" = "-p" ]; then
  echo "==> 打包 RPM (CPack)"
  cmake --build build-fedora --target package

  echo "==> 收集产物到 packaging/dist/"
  mkdir -p packaging/dist
  find build-fedora -maxdepth 1 -name 'flare-client_*.rpm' -type f \
    -exec cp -f {} packaging/dist/ \;

  echo "==> 打包后自动清理（每类保留最新 3 个）"
  ./packaging/cleanup-old-builds.sh

  echo "==> RPM 产物:"
  ls -lh packaging/dist/flare-client_*.rpm
fi

echo "==> 产物: build-fedora/flare-client"
ls -lh build-fedora/flare-client
