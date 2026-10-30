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

# 内置引擎：GUI 包必须自带引擎，终端用户不该自己装 Node 或对版本。
# 这里从同级 flare 仓库现编，避免沿用 CMakeCache 里上一次的旧二进制
# （曾经因为缓存里指到 /tmp/flare-engine，打出来的 RPM 内置的是过期引擎：
#  引擎不下发 createdAt，消息时间戳整排消失）。
CLIENT_VER="$(grep -m1 'project(.*VERSION' CMakeLists.txt | sed 's/[^0-9.]*//g')"
ENGINE_BIN="${FLARE_ENGINE_BINARY:-}"
if [ -z "$ENGINE_BIN" ] && [ -f ../flare/cmd/flare/main.go ]; then
  mkdir -p dist
  ENGINE_BIN="$PWD/dist/flare-engine-linux-$(uname -m)"
  echo "==> 构建引擎 (../flare, 版本 $CLIENT_VER)"
  ( cd ../flare && PATH=/usr/bin:$PATH \
      go build -trimpath -ldflags "-s -w -X main.Version=$CLIENT_VER" \
      -o "$ENGINE_BIN" ./cmd/flare )
fi
if [ -n "$ENGINE_BIN" ]; then
  [ -x "$ENGINE_BIN" ] || { echo "!! 引擎不可执行: $ENGINE_BIN" >&2; exit 1; }
  echo "==> 内置引擎: $ENGINE_BIN"
else
  echo "!! 未找到引擎，GUI 包装完只能用外部 flare —— 仅限开发调试" >&2
fi

echo "==> 配置 CMake (Release)"
cmake -B build-fedora -DCMAKE_BUILD_TYPE=Release ${ENGINE_BIN:+-DFLARE_ENGINE_BINARY="$ENGINE_BIN"}

echo "==> 构建"
cmake --build build-fedora -j"$(nproc)"

echo "==> 运行测试 (TDD)"
# 无图形环境（CI、SSH、后台打包）也要能跑：显式用 offscreen 平台，
# 否则 Qt 拿不到 DISPLAY 会 Subprocess aborted，看起来像测试全红。
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
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
