// embed.go —— 把 Go 引擎内嵌进外壳，实现「一个 exe 双击就能用」
//
// 设计要点：
//  1. 外壳与引擎同为 Go 1.20 编译，零 .NET、零 Node、零安装；
//  2. 引擎不在内存里跑，首次启动释放到 %LOCALAPPDATA%\Flare\engine\ 再拉起，
//     文件名带内容校验，升级后自动换新文件；
//  3. 只有「同目录找不到引擎」时才释放，方便把引擎单独替换掉排查问题。
package main

import (
	_ "embed"
	"crypto/sha256"
	"encoding/hex"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

//go:embed engine/flare.exe
var engineBlob []byte

// userDataDir 返回 %LOCALAPPDATA%\Flare（Win7 起就有 LOCALAPPDATA）
func userDataDir() string {
	base := os.Getenv("LOCALAPPDATA")
	if base == "" {
		base = os.Getenv("APPDATA")
	}
	if base == "" {
		if h, err := os.UserHomeDir(); err == nil {
			base = filepath.Join(h, "AppData", "Local")
		}
	}
	if base == "" {
		base = os.TempDir()
	}
	return filepath.Join(base, "Flare")
}

// releaseEmbedded 把内嵌引擎落盘，返回可执行路径
func releaseEmbedded() (string, error) {
	dir := filepath.Join(userDataDir(), "engine")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return "", err
	}

	sum := sha256.Sum256(engineBlob)
	name := "flare-" + strconv.Itoa(len(engineBlob)) + "-" +
		hex.EncodeToString(sum[:6]) + ".exe"
	dst := filepath.Join(dir, name)

	if fi, err := os.Stat(dst); err == nil && fi.Size() == int64(len(engineBlob)) {
		return dst, nil
	}

	// 先写 .tmp 再改名，防止断电/杀软中断留下半截 exe
	tmp := dst + ".tmp"
	if err := os.WriteFile(tmp, engineBlob, 0o755); err != nil {
		return "", err
	}
	_ = os.Remove(dst)
	if err := os.Rename(tmp, dst); err != nil {
		return "", err
	}

	// 清理旧版本引擎（同目录其它 flare-*.exe）
	if ents, err := os.ReadDir(dir); err == nil {
		for _, it := range ents {
			n := it.Name()
			if n == name || it.IsDir() {
				continue
			}
			if strings.HasPrefix(n, "flare-") && strings.HasSuffix(n, ".exe") {
				_ = os.Remove(filepath.Join(dir, n))
			}
			if strings.HasSuffix(n, ".exe.tmp") {
				_ = os.Remove(filepath.Join(dir, n))
			}
		}
	}
	return dst, nil
}
