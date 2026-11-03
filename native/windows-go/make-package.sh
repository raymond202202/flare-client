#!/usr/bin/env bash
# make-package.sh —— 一键构建 + 打 Windows 发行包（免 .NET、免 Node、免安装）
#
# 用法：bash make-package.sh [输出目录，默认 /tmp/pkgwin]
#
# ── 两套版本号（依 native/protocol/render-spec.md 3.1，不得合并）────────────
#   客户端版本：本脚本从仓库根 CMakeLists.txt 的 project(flare-client VERSION …) 读取，
#               构建时用 -X main.ClientVersion= 注入外壳。界面右下角前半段。
#   引擎版本  ：引擎仓构建时 -X main.Version= 注入，运行时由 version 帧返回。界面后半段。
#   两套号不要求相同，各自独立发版。界面上永远写成「客户端 vX · 内核 flare vY」。
#
# ── Win7 兼容的引擎构建方式（2026-11 起依赖链要求 Go 1.26，不能靠 Go 1.20）──
#   1) 复制一份 GOROOT，避免改系统目录：cp -a /usr/lib/golang /tmp/goroot-win7
#   2) 打仓库自带的 Win7 补丁：python3 tools/patch-goroot-win7.py /tmp/goroot-win7
#   3) GOROOT=/tmp/goroot-win7 PATH=$GOROOT/bin:$PATH，然后：
#      cd flare && CGO_ENABLED=0 GOOS=windows GOARCH=amd64 \
#        go build -trimpath -ldflags "-s -w -X main.Version=$ENGINE_VERSION" \
#        -o <本目录>/engine/flare.exe ./cmd/flare
#
# ── 产物结构（终端用户只看到一个 Flare.exe，不给选择）───────────────────────
#   Flare-Windows/Flare.exe          64 位（Win7~11 通用）
#   Flare-Windows/Flare-32bit.exe    32 位老机器
#   Flare-Windows/Flare-ARM64.exe    ARM64 Windows
#   Flare-Windows/README.txt         中文说明
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-/tmp/pkgwin}"
SRC=/tmp/win32
NAME=Flare-Windows

# ---- 客户端版本：唯一来源 = 仓库根 CMakeLists.txt ----
ROOT_CMAKE="$HERE/../../CMakeLists.txt"
if ! CLIENT_VERSION=$(grep -oP 'project\(\s*flare-client\s+VERSION\s+\K[0-9][0-9.]*' "$ROOT_CMAKE" | head -1); then
  echo "× 读不到客户端版本：$ROOT_CMAKE 里找不到 project(flare-client VERSION ...)" >&2
  exit 1
fi
[ -n "$CLIENT_VERSION" ] || { echo "× 客户端版本为空，拒绝构建（不要产出假版本包）" >&2; exit 1; }
echo "客户端版本 = $CLIENT_VERSION（来源 $ROOT_CMAKE）"

# ---- 引擎版本：优先读构建引擎时写下的版本文件，回退到二进制里的项目版本串 ----
ENGINE_BIN="$HERE/engine/flare.exe"
ENGINE_VERSION=""
if [ -f "$HERE/engine/VERSION" ]; then
  ENGINE_VERSION=$(tr -d ' \n\r' < "$HERE/engine/VERSION")
  echo "引擎版本   = $ENGINE_VERSION（来源 engine/VERSION）"
elif [ -f "$ENGINE_BIN" ]; then
  # 注意：不能抓任意 x.y.z，二进制里有依赖库的版本（如 3.54.1）；
  # 只认本项目的 0.x.y 版本形态，取最大值。
  ENGINE_VERSION=$(strings "$ENGINE_BIN" | grep -oP '\b0\.\d+\.\d+\b' | sort -V | tail -1 || true)
  echo "引擎版本   = ${ENGINE_VERSION:-未识别}（从二进制推断，建议补 engine/VERSION）"
else
  echo "! 未找到 $ENGINE_BIN —— 外壳能跑但没有引擎，先按上面注释构建引擎"
fi

# ---- 构建三架构外壳（注入客户端版本）----
# 必须在包目录内用 "." 构建：go build <绝对路径> 的包名是 command-line-arguments，
# 会让 -X main.ClientVersion 静默失效（等于产出假版本包）。
build_shell() { # $1=GOARCH $2=产物名
  local arch="$1" out="$2"
  ( cd "$HERE" && GOOS=windows GOARCH="$arch" CGO_ENABLED=0 \
      go build -trimpath -ldflags "-s -w -H windowsgui -X main.ClientVersion=$CLIENT_VERSION" \
      -o "$SRC/$out" . )
  # 防假版本包：产物里必须能搜到客户端版本号
  # 坑：set -o pipefail 下不能写 `strings | grep -q`（grep -q 命中即退出，
  #     strings 收到 SIGPIPE，管道返回 141，被误判成"没找到"）。
  local hits
  hits=$(strings "$SRC/$out" | grep -cF "$CLIENT_VERSION" || true)
  if [ "${hits:-0}" -eq 0 ]; then
    echo "× $out 里没有注入客户端版本 $CLIENT_VERSION，拒绝打包" >&2
    exit 1
  fi
  echo "  ✓ $out  $(stat -c%s "$SRC/$out") B  客户端 v$CLIENT_VERSION"
}

if command -v go >/dev/null 2>&1; then
  mkdir -p "$SRC"
  echo "构建外壳（三架构）："
  build_shell amd64 flare-x64.exe
  build_shell 386   flare-x86.exe
  build_shell arm64 flare-arm64.exe
else
  echo "! 找不到 go，跳过构建，直接用 $SRC 里已有产物"
fi

rm -rf "$OUT"; mkdir -p "$OUT/$NAME"
install -m 0755 "$SRC/flare-x64.exe"   "$OUT/$NAME/Flare.exe"
install -m 0755 "$SRC/flare-x86.exe"   "$OUT/$NAME/Flare-32bit.exe"
install -m 0755 "$SRC/flare-arm64.exe" "$OUT/$NAME/Flare-ARM64.exe"

cat > "$OUT/$NAME/README.txt" <<EOF
Flare（Windows 版）
====================================

版本：客户端 v${CLIENT_VERSION}${ENGINE_VERSION:+  ·  内核 flare v${ENGINE_VERSION}}


一、怎么用
   双击 Flare.exe，等一两秒窗口就出来。
   不用装 .NET，不用装 Node，不用装任何运行库，不用联网下载东西。


二、第一次双击可能出现蓝色提示
   Windows 可能弹一个蓝色的「Windows 已保护你的电脑」。
   这是因为程序没有花钱买数字签名证书，不是有毒。
   点「更多信息」→「仍要运行」，就能打开，以后不会再弹。
   （Windows 7 上不会出现这个提示。）


三、第一次打开要填 Key
   程序会自己跳到「连接设置」页面，
   把 Key 粘到框里，点「保存」，然后就能聊天了。
   以后不用再填。想改：点左下角「设置」。


四、32 位和 ARM 电脑
   如果你的电脑是 32 位系统（很老的机器），
   改用 Flare-32bit.exe，用法完全一样。
   如果是 ARM 的 Windows（骁龙那类轻薄本），
   用 Flare-ARM64.exe，用法也完全一样。


五、支持哪些系统
   Windows 7 / 8 / 8.1 / 10 / 11 都能用（同一个版本，不是专版）。
   64 位用 Flare.exe，32 位用 Flare-32bit.exe。
   不需要管理员权限，没有安装过程。


六、聊天记录存在哪
   C:\Users\你的用户名\AppData\Local\Flare
   换电脑时把这个文件夹拷走就行。


七、不想用了
   直接删掉 exe 就能卸载。
   不写注册表，不留后台服务，不弹广告。


八、关于版本号（不用管，看着好看）
   窗口右下角会写「客户端 vX · 内核 flare vY」。
   X 是这个 Flare 程序的版本，Y 是里面那个引擎的版本。
   两个号不一样是正常的，不用管它。
EOF

cd "$OUT" && zip -r -q "$NAME.zip" "$NAME"
ls -l "$OUT/$NAME.zip"
echo "--- 包内 ---"
unzip -l "$OUT/$NAME.zip"
echo
echo "客户端 v$CLIENT_VERSION${ENGINE_VERSION:+ · 内核 flare v$ENGINE_VERSION}"
