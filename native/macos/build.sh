#!/usr/bin/env bash
# build.sh —— macOS 原生壳构建（不需要 Xcode 工程，直接用系统 swiftc）
#
# 用法（在 macOS 上执行）：
#   ./build.sh                       # 只构建 .app（引擎不内置，运行时找系统里的 flare）
#   ENGINE_BIN=/path/to/flare ./build.sh   # 构建内置引擎的 GUI 包（单文件交付形态）
#   VERSION=x.y.z ./build.sh          # 覆盖版本号（默认取客户端 CMakeLists 的 project VERSION）
#
# 产物：build/Flare.app
set -euo pipefail
cd "$(dirname "$0")"

APP_NAME="Flare"
# 版本号只有一处来源：客户端仓库 CMakeLists.txt 的 project(flare-client VERSION x.y.z)。
# 这里曾写死 0.6.163，导致 .app 的 Info.plist 报假版本。
if [ -z "${VERSION:-}" ]; then
  VERSION="$(grep -oE 'project\([^)]* VERSION [0-9]+\.[0-9]+\.[0-9]+' \
             "$(dirname "$0")/../../CMakeLists.txt" | head -1 |
             grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"
  [ -n "$VERSION" ] || { echo "无法从 CMakeLists.txt 解析版本号，请用 VERSION=x.y.z 显式指定" >&2; exit 1; }
fi
OUT="${OUT:-build}"
BUNDLE="$OUT/$APP_NAME.app"

echo "==> 清理 $OUT"
rm -rf "$OUT"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"

if ! command -v swiftc >/dev/null 2>&1; then
    echo "!! 找不到 swiftc，请先安装 Command Line Tools: xcode-select --install" >&2
    exit 1
fi

ARCH="$(uname -m)"
TARGET="$ARCH-apple-macos13.0"
echo "==> 编译 Swift 源码（target=$TARGET）"
swiftc -O -parse-as-library -target "$TARGET" \
    -o "$BUNDLE/Contents/MacOS/$APP_NAME" \
    Sources/*.swift

echo "==> 写入 Info.plist（版本 $VERSION）"
cp Info.plist "$BUNDLE/Contents/Info.plist"
sed -i '' "s/__VERSION__/$VERSION/g" "$BUNDLE/Contents/Info.plist"

if [ -n "${ENGINE_BIN:-}" ]; then
    if [ ! -x "$ENGINE_BIN" ]; then
        echo "!! ENGINE_BIN=$ENGINE_BIN 不可执行" >&2
        exit 1
    fi
    echo "==> 内置引擎 $(basename "$ENGINE_BIN")"
    # 必须放 Resources/bin：APFS 大小写不敏感，放 MacOS/flare 会覆盖主程序 Flare
    mkdir -p "$BUNDLE/Contents/Resources/bin"
    cp "$ENGINE_BIN" "$BUNDLE/Contents/Resources/bin/flare"
    chmod +x "$BUNDLE/Contents/Resources/bin/flare"
fi

echo "==> 临时签名（ad-hoc）"
codesign --force --deep --sign - "$BUNDLE" 2>/dev/null || echo "   （codesign 跳过，本机运行通常不影响）"

echo "==> 体积"
du -sh "$BUNDLE"
echo "==> 完成：$BUNDLE"
echo "   启动：open $BUNDLE"
