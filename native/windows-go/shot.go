package main

// 自截图模块：把窗口客户区存成 BMP。
//
// 为什么需要它：在没有 Windows 真机的环境里（比如 Linux + wine），
// 外部截图工具拿不到被合成器重定向的窗口像素（X_GetImage → BadMatch）。
// 让程序自己用 GDI 抓自己的窗口，是唯一稳定、跨平台一致的取证手段。
//
// 用法（对终端用户隐藏，仅排查用）：
//   Flare.exe --shot=C:\temp\flare.bmp
// 启动后约 3 秒自动把窗口画面存成 BMP 并退出，用户把 bmp 发回来即可。

import (
	"encoding/binary"
	"fmt"
	"os"
	"strconv"
	"strings"
	"syscall"
	"time"
	"unsafe"
)

const (
	wmShot       = WM_APP + 3
	dibRGBColors = 0
	shotDelayMs  = 3000
)

var pGetDIBits = gdi32.NewProc("GetDIBits")

type bitmapInfoHeader struct {
	BiSize          uint32
	BiWidth         int32
	BiHeight        int32
	BiPlanes        uint16
	BiBitCount      uint16
	BiCompression   uint32
	BiSizeImage     uint32
	BiXPelsPerMeter int32
	BiYPelsPerMeter int32
	BiClrUsed       uint32
	BiClrImportant  uint32
}

// shotPath 非空表示本次运行是截图模式。
var shotPath string
var shotDelay = time.Duration(shotDelayMs) * time.Millisecond

// trace 把截图流程的里程碑追加到 <shotPath>.trace.txt。
// 截图失败时没有别的可见输出（GUI 子系统无 stdout），只能靠落盘取证。
func trace(msg string) {
	if shotPath == "" {
		return
	}
	f, err := os.OpenFile(shotPath+".trace.txt", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0o644)
	if err != nil {
		return
	}
	defer f.Close()
	fmt.Fprintf(f, "%s %s\r\n", time.Now().Format("15:04:05.000"), msg)
}

// parseShotArg 解析 --shot=<文件> / --shot <文件> / --shot-delay=<毫秒>。
func parseShotArg() {
	args := os.Args[1:]
	for i := 0; i < len(args); i++ {
		a := args[i]
		switch {
		case strings.HasPrefix(a, "--shot="):
			shotPath = strings.TrimPrefix(a, "--shot=")
		case a == "--shot" && i+1 < len(args):
			shotPath = args[i+1]
			i++
		case strings.HasPrefix(a, "--shot-delay="):
			if ms, err := strconv.Atoi(strings.TrimPrefix(a, "--shot-delay=")); err == nil && ms > 0 {
				shotDelay = time.Duration(ms) * time.Millisecond
			}
		}
	}
	shotPath = strings.TrimSpace(shotPath)
	// 诊断：截图模式先把收到的 argv 落到目标旁边，
	// 用来区分「参数没传进来」和「截图本身失败」——wine/壳层转发都可能吞参数。
	if shotPath != "" {
		_ = os.WriteFile(shotPath+".arg.txt",
			[]byte(strings.Join(os.Args, "\r\n")), 0o644)
	}
	if shotPath != "" {
		// 相对路径落到 exe 同目录，避免工作目录不确定
		if !strings.ContainsAny(shotPath, `:\/`) {
			if exe, err := os.Executable(); err == nil {
				dir := exe
				if idx := strings.LastIndexAny(dir, `\/`); idx > 0 {
					dir = dir[:idx]
				}
				shotPath = dir + `\` + shotPath
			}
		}
	}
}

// armShot 在窗口显示后延迟发一条 wmShot，让首帧绘制和引擎连接都完成。
func armShot(hwnd syscall.Handle) {
	if shotPath == "" || hwnd == 0 {
		return
	}
	go func() {
		trace("armShot: 等待 " + shotDelay.String())
		time.Sleep(shotDelay)
		r, _, e := pPostMessageW.Call(uintptr(hwnd), wmShot, 0, 0)
		trace(fmt.Sprintf("armShot: PostMessage hwnd=%d ret=%d err=%v", hwnd, r, e))
	}()
}

// takeShot 截图 → 关闭窗口 → 进程退出。
func takeShot(hwnd syscall.Handle) {
	trace(fmt.Sprintf("takeShot: 开始 hwnd=%d", hwnd))
	if shotPath != "" {
		if err := saveWindowShot(hwnd, shotPath); err != nil {
			trace("takeShot: saveWindowShot 失败 " + err.Error())
		} else {
			trace("takeShot: 截图已保存 " + shotPath)
		}
		writeFontReport(shotPath + ".font.txt")
	}
	pPostMessageW.Call(uintptr(hwnd), WM_CLOSE, 0, 0)
}

// writeFontReport 把字体探测结果写到截图旁边，方便在陌生机器（例如老 Win7）上
// 直接看出界面到底用了哪个字体，不用装任何调试工具。
func writeFontReport(path string) {
	var b strings.Builder
	b.WriteString("选中字体: " + chosenFont + "\r\n")
	b.WriteString("探测过程:\r\n")
	for _, l := range fontProbe {
		b.WriteString("  " + l + "\r\n")
	}
	_ = os.WriteFile(path, []byte(b.String()), 0o644)
}

// saveWindowShot 用 BitBlt 抓窗口客户区，写成 24 位 BMP（兼容画图、飞书预览）。
func saveWindowShot(hwnd syscall.Handle, path string) error {
	var r RECT
	pGetClientRect.Call(uintptr(hwnd), uintptr(unsafe.Pointer(&r)))
	w, h := r.Right, r.Bottom
	if w <= 0 || h <= 0 {
		return nil
	}

	hdc, _, _ := pGetDC.Call(uintptr(hwnd))
	if hdc == 0 {
		return nil
	}
	defer pReleaseDC.Call(uintptr(hwnd), hdc)

	mem, _, _ := pCreateCompatibleDC.Call(hdc)
	if mem == 0 {
		return nil
	}
	bmp, _, _ := pCreateCompatibleBitmap.Call(hdc, uintptr(w), uintptr(h))
	if bmp == 0 {
		pDeleteDC.Call(mem)
		return nil
	}
	old, _, _ := pSelectObject.Call(mem, bmp)
	pBitBlt.Call(mem, 0, 0, uintptr(w), uintptr(h), hdc, 0, 0, SRCCOPY)

	bmi := bitmapInfoHeader{
		BiSize:        uint32(unsafe.Sizeof(bitmapInfoHeader{})),
		BiWidth:       w,
		BiHeight:      -h, // 负高度 = 自上而下，省一次翻转
		BiPlanes:      1,
		BiBitCount:    32,
		BiCompression: 0, // BI_RGB
	}
	stride := int(w) * 4
	buf := make([]byte, stride*int(h))
	pGetDIBits.Call(mem, bmp, 0, uintptr(h),
		uintptr(unsafe.Pointer(&buf[0])),
		uintptr(unsafe.Pointer(&bmi)), dibRGBColors)

	pSelectObject.Call(mem, old)
	pDeleteObject.Call(bmp)
	pDeleteDC.Call(mem)

	// 32bpp BGRA → 24bpp BGR，行按 4 字节对齐
	// buf 是自上而下（GetDIBits 用了 -h），而 BMP 文件正高度 = 自下而上，
	// 所以这里按倒序取源行，写出的 pix 才是标准 bottom-up 文件。
	rowSize := ((int(w)*3 + 3) / 4) * 4
	pix := make([]byte, rowSize*int(h))
	for y := 0; y < int(h); y++ {
		src := buf[(int(h)-1-y)*stride:]
		dst := pix[y*rowSize:]
		for x := 0; x < int(w); x++ {
			dst[x*3+0] = src[x*4+0]
			dst[x*3+1] = src[x*4+1]
			dst[x*3+2] = src[x*4+2]
		}
	}

	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()

	fh := make([]byte, 54)
	fh[0], fh[1] = 'B', 'M'
	binary.LittleEndian.PutUint32(fh[2:], uint32(54+len(pix)))
	binary.LittleEndian.PutUint32(fh[10:], 54)
	binary.LittleEndian.PutUint32(fh[14:], 40)
	binary.LittleEndian.PutUint32(fh[18:], uint32(w))
	binary.LittleEndian.PutUint32(fh[22:], uint32(h))
	binary.LittleEndian.PutUint16(fh[26:], 1)
	binary.LittleEndian.PutUint16(fh[28:], 24)
	binary.LittleEndian.PutUint32(fh[34:], uint32(len(pix)))
	binary.LittleEndian.PutUint32(fh[38:], 2835)
	binary.LittleEndian.PutUint32(fh[42:], 2835)

	if _, err := f.Write(fh); err != nil {
		return err
	}
	_, err = f.Write(pix)
	return err
}
