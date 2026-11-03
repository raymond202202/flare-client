#!/usr/bin/env bash
# make-package.sh —— 打 Windows 发行包（不下载任何东西、不做安装、免 .NET 免 Node）
#
# 用法：bash make-package.sh [输出目录，默认 /tmp/pkgwin]
#
# 前提：已用 Go 1.20 交叉编译出三架构产物（Go 1.20 是最后一个支持 Win7 的版本）：
#   export GOOS=windows CGO_ENABLED=0
#   GOARCH=amd64 go build -trimpath -ldflags="-s -w -H windowsgui" -o /tmp/win32/flare-x64.exe .
#   GOARCH=386   go build ... -o /tmp/win32/flare-x86.exe .
#   GOARCH=arm64 go build ... -o /tmp/win32/flare-arm64.exe .
#
# 产物结构（终端用户只看到一个 Flare.exe，不给选择）：
#   Flare-Windows/Flare.exe          64 位（Win7~11 通用，含 ARM64 模拟）
#   Flare-Windows/Flare-32bit.exe    32 位老机器
#   Flare-Windows/README.txt         中文说明
set -euo pipefail

OUT="${1:-/tmp/pkgwin}"
SRC=/tmp/win32
NAME=Flare-Windows

rm -rf "$OUT"; mkdir -p "$OUT/$NAME"

install -m 0755 "$SRC/flare-x64.exe"   "$OUT/$NAME/Flare.exe"
install -m 0755 "$SRC/flare-x86.exe"   "$OUT/$NAME/Flare-32bit.exe"

cat > "$OUT/$NAME/README.txt" <<'EOF'
Flare（Windows 版）
====================================

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


四、32 位的老电脑
   如果你的电脑是 32 位系统（很老的机器），
   改用 Flare-32bit.exe，用法完全一样。


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
EOF

cd "$OUT" && zip -r -q "$NAME.zip" "$NAME"
ls -l "$OUT/$NAME.zip"
echo "--- 包内 ---"
unzip -l "$OUT/$NAME.zip"
