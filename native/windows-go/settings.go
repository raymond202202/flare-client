// settings.go —— 连接设置页（同窗口内视图切换，不用弹窗）
//
// 目标：用户不需要知道 .env、Base URL、模型名这些东西。
// 界面上只有一个输入框 + 一个保存按钮，剩下的全部由程序写好。
package main

import (
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"unsafe"
)

const (
	viewChat  = 0
	viewSetup = 1

	// winuser.h: ES_PASSWORD / ES_AUTOHSCROLL
	esPassword    = 0x0020
	esAutoHScroll = 0x0080
)

var (
	hwndKey syscall.Handle
	hInstG  uintptr

	rcGearBtn   RECT
	rcKeyEdit   RECT
	rcSaveKey   RECT
	rcCancelKey RECT
)

// flareHome 与引擎保持一致：FLARE_HOME 优先，否则 ~/.flare
func flareHome() string {
	if h := strings.TrimSpace(os.Getenv("FLARE_HOME")); h != "" {
		return h
	}
	h, err := os.UserHomeDir()
	if err != nil {
		return ""
	}
	return filepath.Join(h, ".flare")
}

func envFile() string { return filepath.Join(flareHome(), ".env") }

// readAPIKey 从 ~/.flare/.env 里取出已保存的 key
func readAPIKey() string {
	b, err := os.ReadFile(envFile())
	if err != nil {
		return ""
	}
	for _, ln := range strings.Split(string(b), "\n") {
		t := strings.TrimSpace(ln)
		if t == "" || strings.HasPrefix(t, "#") {
			continue
		}
		i := strings.Index(t, "=")
		if i <= 0 {
			continue
		}
		name := strings.TrimSpace(t[:i])
		val := strings.Trim(strings.TrimSpace(t[i+1:]), `"'`)
		if name == "LLM_API_KEY" || name == "DEEPSEEK_API_KEY" {
			return val
		}
	}
	return ""
}

// saveAPIKey 写入 ~/.flare/.env，保留用户已有的其它配置
func saveAPIKey(key string) error {
	dir := flareHome()
	if dir == "" {
		return os.ErrNotExist
	}
	if err := os.MkdirAll(dir, 0o700); err != nil {
		return err
	}
	path := envFile()

	var keep []string
	if b, err := os.ReadFile(path); err == nil {
		for _, ln := range strings.Split(string(b), "\n") {
			t := strings.TrimSpace(ln)
			if t == "" {
				continue
			}
			name := t
			if i := strings.Index(t, "="); i > 0 {
				name = strings.TrimSpace(t[:i])
			}
			// 只重写这两项，其余（含 DEEPSEEK_API_KEY 等）原样保留
			if name == "LLM_API_KEY" || name == "LLM_MODEL" {
				continue
			}
			keep = append(keep, ln)
		}
	}
	out := append(keep, "LLM_API_KEY="+key, "LLM_MODEL=deepseek-chat")
	data := strings.Join(out, "\n") + "\n"

	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, []byte(data), 0o600); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}

func layoutSetup(cr RECT) {
	left := int32(sidebarW) + pad*3
	top := int32(titleBarH + 56)

	// 输入框宽度跟着窗口走，绝不写死：窗口被缩到最小时也不能出屏
	w := cr.Right - left - pad*3
	if w < 260 {
		w = 260
	}

	rcKeyEdit = RECT{left, top + 96, left + w, top + 130}
	rcSaveKey = RECT{left, top + 152, left + 108, top + 190}
	rcCancelKey = RECT{left + 120, top + 152, left + 228, top + 190}
}

// drawSetup 画设置页：只覆盖右侧主区，标题栏和侧栏照常显示
func drawSetup(hdc syscall.Handle, cr RECT) {
	fillRect(hdc, RECT{int32(sidebarW), titleBarH, cr.Right, cr.Bottom}, colBg)
	layoutSetup(cr)

	left := rcKeyEdit.Left

	drawText(hdc, "连接设置", RECT{left, titleBarH + 56, cr.Right - pad, titleBarH + 86},
		fontTitle, colText, DT_LEFT|DT_VCENTER|DT_SINGLELINE)

	drawText(hdc, "把 API Key 粘贴到下面，保存后就能开始对话。",
		RECT{left, titleBarH + 90, cr.Right - pad, titleBarH + 114},
		fontUI, colTextDim, DT_LEFT|DT_VCENTER|DT_SINGLELINE)

	drawText(hdc, "其它配置会自动写好，不用管。",
		RECT{left, titleBarH + 114, cr.Right - pad, titleBarH + 138},
		fontUI, colTextDim, DT_LEFT|DT_VCENTER|DT_SINGLELINE)

	drawText(hdc, "API Key", RECT{left, rcKeyEdit.Top - 26, left + 200, rcKeyEdit.Top - 4},
		fontUIBold, colText, DT_LEFT|DT_VCENTER|DT_SINGLELINE)

	// 保存按钮
	saveHover := st.HoverBtn == 7
	sc := colPrimary
	if saveHover {
		sc = colPrimaryDk
	}
	fillRect(hdc, rcSaveKey, sc)
	drawText(hdc, "保存", rcSaveKey, fontUIBold, colSurface, DT_CENTER|DT_VCENTER|DT_SINGLELINE)

	// 取消按钮
	cancelHover := st.HoverBtn == 8
	cc := colHover
	if cancelHover {
		cc = colBorder
	}
	fillRect(hdc, rcCancelKey, cc)
	drawText(hdc, "取消", rcCancelKey, fontUI, colText, DT_CENTER|DT_VCENTER|DT_SINGLELINE)

	// 状态/提示
	if st.Status != "" {
		col := colTextMute
		if st.ErrorHint {
			col = colDanger
		}
		drawText(hdc, st.Status, RECT{left, rcSaveKey.Bottom + 14, cr.Right - pad, rcSaveKey.Bottom + 40},
			fontSmall, col, DT_LEFT|DT_VCENTER|DT_SINGLELINE)
	}
}

// openSetup 切到设置页并创建输入框
func openSetup() {
	st.View = viewSetup
	st.ErrorHint = false
	cr := clientRect(hwndMain)
	layoutSetup(cr)

	if hwndKey == 0 {
		title := u16("")
		cls := u16("EDIT")
		h, _, _ := pCreateWindowExW.Call(
			0,
			uintptr(unsafe.Pointer(cls)),
			uintptr(unsafe.Pointer(title)),
			uintptr(WS_CHILD|WS_VISIBLE|WS_BORDER|esPassword|esAutoHScroll),
			uintptr(rcKeyEdit.Left), uintptr(rcKeyEdit.Top),
			uintptr(rcKeyEdit.Right-rcKeyEdit.Left), uintptr(rcKeyEdit.Bottom-rcKeyEdit.Top),
			uintptr(hwndMain), 0, hInstG, 0)
		hwndKey = syscall.Handle(h)
		if hwndKey != 0 {
			pSendMessageW.Call(uintptr(hwndKey), WM_SETFONT, fontInput, 1)
		}
	}
	if hwndKey != 0 {
		pSetWindowPos.Call(uintptr(hwndKey), 0,
			uintptr(rcKeyEdit.Left), uintptr(rcKeyEdit.Top),
			uintptr(rcKeyEdit.Right-rcKeyEdit.Left), uintptr(rcKeyEdit.Bottom-rcKeyEdit.Top),
			SWP_NOZORDER|SWP_NOACTIVATE)
		pShowWindow.Call(uintptr(hwndKey), SW_SHOW)
		// 预填已保存的 key，方便改
		if k := readAPIKey(); k != "" {
			pSetWindowTextW.Call(uintptr(hwndKey), uintptr(unsafe.Pointer(u16(k))))
		}
		pSetFocus.Call(uintptr(hwndKey))
	}
	// 聊天输入框藏起来，避免两个 EDIT 抢焦点
	if hEdit != 0 {
		pShowWindow.Call(uintptr(hEdit), SW_HIDE)
	}
	st.Status = ""
	pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
}

// closeSetup 切回聊天
func closeSetup() {
	st.View = viewChat
	if hwndKey != 0 {
		pShowWindow.Call(uintptr(hwndKey), SW_HIDE)
	}
	if hEdit != 0 {
		pShowWindow.Call(uintptr(hEdit), SW_SHOW)
	}
	st.ErrorHint = false
	pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
}

// doSaveKey 保存并重启引擎，让新 key 立刻生效
func doSaveKey() {
	if hwndKey == 0 {
		return
	}
	var buf [2048]uint16
	n, _, _ := pGetWindowTextW.Call(uintptr(hwndKey), uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)-1))
	key := strings.TrimSpace(syscall.UTF16ToString(buf[:n]))

	if key == "" {
		st.Status = "请先粘贴 API Key"
		st.ErrorHint = true
		pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
		return
	}
	if strings.ContainsAny(key, " \r\n\t") {
		st.Status = "Key 里不能有空格或换行，请重新复制"
		st.ErrorHint = true
		pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
		return
	}
	if err := saveAPIKey(key); err != nil {
		st.Status = "保存失败：" + err.Error()
		st.ErrorHint = true
		pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
		return
	}

	// 重启引擎，带上新配置
	eng.Stop()
	if err := eng.Start(); err != nil {
		st.Status = "已保存，但引擎启动失败：" + err.Error()
		st.ErrorHint = true
		pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
		return
	}

	closeSetup()
	st.Status = "已保存"
	pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
}
