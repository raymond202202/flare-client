// main.go —— Flare Win32 客户端（纯 syscall 自绘，零 .NET，Win7 SP1+）
package main

import (
	"strconv"
	"syscall"
	"unsafe"
)

// ---------------- 状态 ----------------

type Session struct {
	ID      string
	Title   string
	Updated string
	Preview string
	Count   int
}

type ChatMsg struct {
	Kind   int // 0=user 1=assistant 2=tool 3=system
	Text   string
	Author string
}

type AppState struct {
	Sessions   []Session
	CurSession string
	Msgs       []ChatMsg

	Busy      bool
	Status    string
	ChatScroll int32
	SideScroll int32

	HoverBtn   int // 0 none, 1 min, 2 max, 3 close, 4 new, 5 send, 6 gear, 7 save, 8 cancel
	HoverSide  int
	DragScroll bool

	View      int  // viewChat / viewSetup
	ErrorHint bool // 状态行是否用错误色
}

var (
	hwndMain  syscall.Handle
	hEdit     syscall.Handle
	oldEditProc uintptr
	st        = &AppState{Status: "就绪"}
	dpi       int32 = 96

	// 布局缓存
	rcSendBtn RECT
	rcNewBtn  RECT
	rcInputCard RECT
)

// ---------------- GDI 辅助 ----------------

func fillRect(hdc syscall.Handle, r RECT, color uint32) {
	br, _, _ := pCreateSolidBrush.Call(uintptr(color))
	pFillRect.Call(uintptr(hdc), uintptr(unsafe.Pointer(&r)), br)
	pDeleteObject.Call(br)
}

func roundFill(hdc syscall.Handle, r RECT, color uint32, rad int32) {
	br, _, _ := pCreateSolidBrush.Call(uintptr(color))
	nullPen, _, _ := pGetStockObject.Call(NULL_PEN)
	oldBr, _, _ := pSelectObject.Call(uintptr(hdc), br)
	oldPen, _, _ := pSelectObject.Call(uintptr(hdc), nullPen)
	pRoundRect.Call(uintptr(hdc), uintptr(r.Left), uintptr(r.Top),
		uintptr(r.Right), uintptr(r.Bottom), uintptr(rad), uintptr(rad))
	pSelectObject.Call(uintptr(hdc), oldBr)
	pSelectObject.Call(uintptr(hdc), oldPen)
	pDeleteObject.Call(br)
}

func roundStroke(hdc syscall.Handle, r RECT, color uint32, rad int32) {
	pen, _, _ := pCreatePen.Call(PS_SOLID, 1, uintptr(color))
	nullBr, _, _ := pGetStockObject.Call(NULL_BRUSH)
	oldPen, _, _ := pSelectObject.Call(uintptr(hdc), pen)
	oldBr, _, _ := pSelectObject.Call(uintptr(hdc), nullBr)
	pRoundRect.Call(uintptr(hdc), uintptr(r.Left), uintptr(r.Top),
		uintptr(r.Right), uintptr(r.Bottom), uintptr(rad), uintptr(rad))
	pSelectObject.Call(uintptr(hdc), oldPen)
	pSelectObject.Call(uintptr(hdc), oldBr)
	pDeleteObject.Call(pen)
}

func drawText(hdc syscall.Handle, text string, r RECT, font uintptr, color uint32, flags uint32) int32 {
	pSelectObject.Call(uintptr(hdc), font)
	pSetBkMode.Call(uintptr(hdc), TRANSPARENT)
	pSetTextColor.Call(uintptr(hdc), uintptr(color))
	p16 := u16(text)
	h, _, _ := pDrawTextW.Call(uintptr(hdc), uintptr(unsafe.Pointer(p16)), ^uintptr(0),
		uintptr(unsafe.Pointer(&r)), uintptr(flags))
	return int32(h)
}

func measureText(hdc syscall.Handle, text string, font uintptr, width int32) int32 {
	r := RECT{0, 0, width, 0}
	pSelectObject.Call(uintptr(hdc), font)
	p16 := u16(text)
	h, _, _ := pDrawTextW.Call(uintptr(hdc), uintptr(unsafe.Pointer(p16)), ^uintptr(0),
		uintptr(unsafe.Pointer(&r)), uintptr(DT_LEFT|DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT))
	return int32(h)
}

// ---------------- 绘制 ----------------

func clientRect(h syscall.Handle) RECT {
	var r RECT
	pGetClientRect.Call(uintptr(h), uintptr(unsafe.Pointer(&r)))
	return r
}

func layout(cr RECT) {
	sidebar := int32(sidebarW)
	cardTop := cr.Bottom - inputH
	rcInputCard = RECT{sidebar + pad, cardTop + pad/2, cr.Right - pad, cr.Bottom - pad/2}
	rcSendBtn = RECT{rcInputCard.Right - 60, rcInputCard.Bottom - 46, rcInputCard.Right - 10, rcInputCard.Bottom - 8}
	rcNewBtn = RECT{pad, titleBarH + 10, sidebar - pad, titleBarH + 46}
	// 齿轮固定在侧栏底部（符合「设置」在左下的惯例）
	rcGearBtn = RECT{pad, cr.Bottom - 52, sidebar - pad, cr.Bottom - 12}
	layoutSetup(cr)
}

func drawAll(h syscall.Handle, hdc syscall.Handle) {
	cr := clientRect(h)
	layout(cr)

	// 双缓冲
	mem, _, _ := pCreateCompatibleDC.Call(uintptr(hdc))
	bmp, _, _ := pCreateCompatibleBitmap.Call(uintptr(hdc), uintptr(cr.Right), uintptr(cr.Bottom))
	old, _, _ := pSelectObject.Call(mem, bmp)
	mh := syscall.Handle(mem)

	fillRect(mh, cr, colBg)

	// ---- 标题栏 ----
	tb := RECT{0, 0, cr.Right, titleBarH}
	fillRect(mh, tb, colBg)

	// 应用图标：紫色圆角方块 + F
	iconR := RECT{pad, (titleBarH - 26) / 2, pad + 26, (titleBarH-26)/2 + 26}
	roundFill(mh, iconR, colPrimary, 8)
	drawText(mh, "F", RECT{iconR.Left, iconR.Top + 1, iconR.Right, iconR.Bottom},
		fontUIBold, colSurface, DT_CENTER|DT_VCENTER|DT_SINGLELINE)

	drawText(mh, "Flare", RECT{iconR.Right + 10, 0, iconR.Right + 120, titleBarH},
		fontTitle, colText, DT_LEFT|DT_VCENTER|DT_SINGLELINE)

	// 标题栏按钮（右侧）
	bx := cr.Right
	for i := 3; i >= 1; i-- {
		bx -= btnW
		r := RECT{bx, 0, bx + btnW, titleBarH}
		if st.HoverBtn == i {
			var c uint32 = colHover
			if i == 3 {
				c = colDanger
			}
			fillRect(mh, r, c)
		}
		var fg uint32 = colTextDim
		if i == 3 && st.HoverBtn == i {
			fg = colSurface
		}
		var glyph string
		switch i {
		case 1:
			glyph = "—"
		case 2:
			glyph = "▢"
		case 3:
			glyph = "✕"
		}
		drawText(mh, glyph, r, fontUI, fg, DT_CENTER|DT_VCENTER|DT_SINGLELINE)
	}

	// 标题栏下边框
	fillRect(mh, RECT{0, titleBarH - 1, cr.Right, titleBarH}, colBorder)

	// ---- 侧栏 ----
	sb := RECT{0, titleBarH, int32(sidebarW), cr.Bottom}
	fillRect(mh, sb, colSidebar)
	fillRect(mh, RECT{sb.Right - 1, sb.Top, sb.Right, sb.Bottom}, colBorder)

	// 新对话按钮
	newHover := st.HoverBtn == 4
	nc := colPrimary
	if newHover {
		nc = colPrimaryDk
	}
	roundFill(mh, rcNewBtn, nc, radius)
	drawText(mh, "＋  新对话", rcNewBtn, fontUIBold, colSurface, DT_CENTER|DT_VCENTER|DT_SINGLELINE)

	// 会话列表
	listTop := rcNewBtn.Bottom + 8
	pSetBkMode.Call(uintptr(mh), TRANSPARENT)
	y := listTop - st.SideScroll
	for i, s := range st.Sessions {
		ir := RECT{8, y, int32(sidebarW) - 8, y + itemH - 4}
		if ir.Bottom > titleBarH && ir.Top < cr.Bottom {
			sel := s.ID == st.CurSession
			switch {
			case sel:
				roundFill(mh, ir, colSidebarHi, 8)
				roundStroke(mh, ir, colBorder, 8)
			case st.HoverSide == i:
				roundFill(mh, ir, colHover, 8)
			}
			title := s.Title
			if title == "" {
				title = "未命名对话"
			}
			tc := colText
			if sel {
				tc = colPrimary
			}
			drawText(mh, title, RECT{ir.Left + 12, ir.Top + 8, ir.Right - 12, ir.Top + 30},
				fontUIBold, tc, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS)
			prev := s.Preview
			if prev == "" {
				if s.Count > 0 {
					prev = strconv.Itoa(s.Count) + " 条消息"
				} else {
					prev = "暂无消息"
				}
			}
			drawText(mh, prev, RECT{ir.Left + 12, ir.Top + 28, ir.Right - 12, ir.Bottom - 4},
				fontSmall, colTextMute, DT_LEFT|DT_TOP|DT_SINGLELINE|DT_END_ELLIPSIS)
		}
		y += itemH
	}

	// ---- 主区 ----
	mainR := RECT{int32(sidebarW), titleBarH, cr.Right, cr.Bottom}
	fillRect(mh, mainR, colSurface)

	// 消息区
	chatR := RECT{mainR.Left + pad + 8, mainR.Top + pad, mainR.Right - pad - 8, rcInputCard.Top - pad/2}
	drawMessages(mh, chatR)

	// 输入卡片
	roundFill(mh, rcInputCard, colSurface, radius)
	roundStroke(mh, rcInputCard, colBorder, radius)

	// 发送按钮
	sendHover := st.HoverBtn == 5
	sc := colPrimary
	if sendHover {
		sc = colPrimaryDk
	}
	if st.Busy {
		sc = colDanger
	}
	roundFill(mh, rcSendBtn, sc, 8)
	label := "↑"
	if st.Busy {
		label = "■"
	}
	drawText(mh, label, rcSendBtn, fontUIBold, colSurface, DT_CENTER|DT_VCENTER|DT_SINGLELINE)

	// ---- 设置入口（侧栏底部）----
	if st.HoverBtn == 6 {
		fillRect(mh, rcGearBtn, colHover)
	}
	dotR := RECT{rcGearBtn.Left + 12, rcGearBtn.Top, rcGearBtn.Left + 22, rcGearBtn.Bottom}
	drawText(mh, "●", dotR, fontSmall, colPrimary, DT_LEFT|DT_VCENTER|DT_SINGLELINE)
	drawText(mh, "设置", RECT{rcGearBtn.Left + 26, rcGearBtn.Top, rcGearBtn.Right, rcGearBtn.Bottom},
		fontUI, colTextDim, DT_LEFT|DT_VCENTER|DT_SINGLELINE)

	// ---- 状态栏（左下角小字）----
	statusR := RECT{int32(sidebarW) + pad, cr.Bottom - 20, cr.Right - pad, cr.Bottom}
	if st.Status != "" {
		drawText(mh, st.Status, statusR, fontSmall, colTextMute, DT_RIGHT|DT_VCENTER|DT_SINGLELINE)
	}

	// 设置页叠加在主区之上（侧栏、标题栏保持可见）
	if st.View == viewSetup {
		drawSetup(syscall.Handle(mem), cr)
	}

	pBitBlt.Call(uintptr(hdc), 0, 0, uintptr(cr.Right), uintptr(cr.Bottom), mem, 0, 0, SRCCOPY)
	pSelectObject.Call(mem, old)
	pDeleteObject.Call(bmp)
	pDeleteDC.Call(mem)
}

func drawMessages(hdc syscall.Handle, area RECT) {
	if len(st.Msgs) == 0 {
		r := RECT{area.Left, area.Top + 80, area.Right, area.Top + 140}
		drawText(hdc, "开始新的对话", r, fontTitle, colTextMute, DT_CENTER|DT_TOP|DT_SINGLELINE)
		r2 := RECT{area.Left, area.Top + 116, area.Right, area.Top + 160}
		drawText(hdc, "在下面输入问题，回车发送", r2, fontSmall, colTextMute, DT_CENTER|DT_TOP|DT_SINGLELINE)
		return
	}

	maxW := area.W() - 120
	// 先算总高
	var total int32
	heights := make([]int32, len(st.Msgs))
	for i, m := range st.Msgs {
		w := maxW
		if m.Kind == 0 {
			w = maxW - 120
		}
		h := measureText(hdc, m.Text, fontUI, w)
		if h < 22 {
			h = 22
		}
		heights[i] = h
		total += h + 22
	}
	// clamp 滚动
	maxScroll := total - area.H() + 16
	if maxScroll < 0 {
		maxScroll = 0
	}
	if st.ChatScroll > maxScroll {
		st.ChatScroll = maxScroll
	}
	if st.ChatScroll < 0 {
		st.ChatScroll = 0
	}

	y := area.Top - st.ChatScroll
	clipR := area
	// 简易剪裁：跳过完全在可视区外的条目
	for i, m := range st.Msgs {
		bh := heights[i] + 10
		ir := RECT{area.Left, y, area.Right, y + bh}
		if ir.Bottom >= clipR.Top && ir.Top <= clipR.Bottom {
			if m.Kind == 0 {
				// 用户：右对齐紫色气泡
				tw := maxW - 120
				th := heights[i]
				bw := tw
				br := RECT{area.Right - bw, y, area.Right, y + th + 20}
				roundFill(hdc, br, colUserBg, radius)
				tr := RECT{br.Left + 14, br.Top + 10, br.Right - 14, br.Bottom - 10}
				drawText(hdc, m.Text, tr, fontUI, colUserText, DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX)
			} else {
				th := heights[i]
				br := RECT{area.Left, y, area.Left + maxW + 120, y + th + 20}
				roundFill(hdc, br, colBubble, radius)
				tr := RECT{br.Left + 14, br.Top + 10, br.Right - 14, br.Bottom - 10}
				drawText(hdc, m.Text, tr, fontUI, colText, DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX)
			}
		}
		y += bh + 12
		_ = m
	}
}

// ---------------- 事件处理 ----------------

func handleEngineEvents() {
	evs := eng.Drain()
	if len(evs) == 0 {
		return
	}
	changed := false
	for _, m := range evs {
		t, _ := m["type"].(string)
		switch t {
		case "version":
			if v, ok := m["engine"].(string); ok {
				st.Status = "引擎 " + v
			}
		case "sessions":
			if arr, ok := m["sessions"].([]any); ok {
				st.Sessions = st.Sessions[:0]
				for _, it := range arr {
					o, _ := it.(map[string]any)
					s := Session{}
					if v, ok := o["id"].(string); ok {
						s.ID = v
					}
					if v, ok := o["title"].(string); ok {
						s.Title = v
					}
					if v, ok := o["updatedAt"].(string); ok {
						s.Updated = v
					}
					if v, ok := o["preview"].(string); ok {
						s.Preview = v
					}
					if v, ok := o["messageCount"].(float64); ok {
						s.Count = int(v)
					}
					st.Sessions = append(st.Sessions, s)
				}
				changed = true
			}
		case "session_created":
			if sid, ok := m["sessionId"].(string); ok && sid != "" {
				st.CurSession = sid
				st.Msgs = nil
				changed = true
			}
		case "messages":
			if arr, ok := m["messages"].([]any); ok {
				st.Msgs = st.Msgs[:0]
				for _, it := range arr {
					o, _ := it.(map[string]any)
					role, _ := o["role"].(string)
					content, _ := o["content"].(string)
					if content == "" {
						continue
					}
					k := 1
					if role == "user" {
						k = 0
					} else if role == "tool" {
						k = 2
					}
					st.Msgs = append(st.Msgs, ChatMsg{Kind: k, Text: content})
				}
				st.ChatScroll = 1 << 30 // 到底
				changed = true
			}
		case "text":
			if s, ok := m["content"].(string); ok {
				if n := len(st.Msgs); n > 0 && st.Busy && st.Msgs[n-1].Kind == 1 {
					st.Msgs[n-1].Text += s
				} else {
					st.Msgs = append(st.Msgs, ChatMsg{Kind: 1, Text: s})
					st.Busy = true
				}
				st.ChatScroll = 1 << 30
				changed = true
			}
		case "done":
			// 兜底：若增量帧一个都没收到，用 done 里的完整 content
			if full, ok := m["content"].(string); ok && full != "" {
				if n := len(st.Msgs); n == 0 || st.Msgs[n-1].Kind != 1 {
					st.Msgs = append(st.Msgs, ChatMsg{Kind: 1, Text: full})
				}
			}
			st.Busy = false
			st.Status = "就绪"
			if u, ok := m["usage"].(map[string]any); ok {
				if n, ok := u["totalTokens"].(float64); ok {
					st.Status = "就绪 · 本轮 " + strconv.Itoa(int(n)) + " tokens"
				}
			}
			changed = true
			eng.Send(map[string]any{"type": "list_sessions"})
		case "error":
			msg, _ := m["content"].(string)
			if msg == "" {
				msg, _ = m["message"].(string)
			}
			if msg != "" {
				st.Status = "出错：" + msg
			}
			st.Busy = false
			changed = true
		case "tool_call":
			name, _ := m["toolName"].(string)
			if name == "" {
				name, _ = m["name"].(string)
			}
			if name != "" {
				st.Msgs = append(st.Msgs, ChatMsg{Kind: 2, Text: "调用工具：" + name})
				changed = true
			}
		case "tool_result":
			name, _ := m["toolName"].(string)
			if name != "" {
				st.Status = "工具完成：" + name
				changed = true
			}
		case "cancelled":
			st.Busy = false
			st.Status = "已取消"
			changed = true
		case "session_updated", "messages_cleared":
			changed = true
		}
	}
	if changed {
		pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
	}
}

func doSend() {
	var buf [8192]uint16
	n, _, _ := pGetWindowTextW.Call(uintptr(hEdit), uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)-1))
	text := syscall.UTF16ToString(buf[:n])
	if len(text) == 0 {
		return
	}
	// 去掉末尾换行
	for len(text) > 0 && (text[len(text)-1] == '\r' || text[len(text)-1] == '\n') {
		text = text[:len(text)-1]
	}
	if text == "" {
		return
	}
	pSetWindowTextW.Call(uintptr(hEdit), uintptr(unsafe.Pointer(u16(""))))
	st.Msgs = append(st.Msgs, ChatMsg{Kind: 0, Text: text})
	st.Msgs = append(st.Msgs, ChatMsg{Kind: 1, Text: ""})
	st.Busy = true
	st.ChatScroll = 1 << 30
	st.Status = "生成中…"
	eng.Chat(text)
	pInvalidateRect.Call(uintptr(hwndMain), 0, 0)
}

// ---------------- 窗口过程 ----------------

func pointIn(p POINT, r RECT) bool {
	return p.X >= r.Left && p.X <= r.Right && p.Y >= r.Top && p.Y <= r.Bottom
}

func hitTest(sx, sy int32) int {
	cr := clientRect(hwndMain)
	if sy <= titleBarH {
		bx := cr.Right
		for i := 3; i >= 1; i-- {
			bx -= btnW
			if sx >= bx && sx <= bx+btnW {
				return i
			}
		}
		return 0
	}
	if st.View == viewSetup {
		if pointIn(POINT{sx, sy}, rcSaveKey) {
			return 7
		}
		if pointIn(POINT{sx, sy}, rcCancelKey) {
			return 8
		}
		if pointIn(POINT{sx, sy}, rcGearBtn) {
			return 6
		}
		return 0
	}
	if sx <= sidebarW {
		if pointIn(POINT{sx, sy}, rcNewBtn) {
			return 4
		}
		if pointIn(POINT{sx, sy}, rcGearBtn) {
			return 6
		}
		return 0
	}
	if pointIn(POINT{sx, sy}, rcSendBtn) {
		return 5
	}
	return 0
}

func wndProc(h syscall.Handle, msg uint32, wp, lp uintptr) uintptr {
	switch msg {
	case WM_PAINT:
		var ps PAINTSTRUCT
		hdc, _, _ := pBeginPaint.Call(uintptr(h), uintptr(unsafe.Pointer(&ps)))
		drawAll(h, syscall.Handle(hdc))
		pEndPaint.Call(uintptr(h), uintptr(unsafe.Pointer(&ps)))
		return 0

	case WM_ERASEBKGND:
		return 1

	case WM_SIZE:
		relayoutChildren()
		pInvalidateRect.Call(uintptr(h), 0, 0)
		return 0

	case wmEngineEvent:
		handleEngineEvents()
		return 0

	case wmEngineDead:
		st.Status = "引擎已退出"
		st.Busy = false
		pInvalidateRect.Call(uintptr(h), 0, 0)
		return 0

	case WM_MOUSEMOVE:
		x, y := loWord(lp), hiWord(lp)
		ht := hitTest(x, y)
		changed := ht != st.HoverBtn
		st.HoverBtn = ht
		// 侧栏 hover
		cr := clientRect(h)
		hs := -1
		if x <= sidebarW && y > rcNewBtn.Bottom+8 && y < cr.Bottom {
			idx := int((y - (rcNewBtn.Bottom + 8) + st.SideScroll) / itemH)
			if idx >= 0 && idx < len(st.Sessions) {
				hs = idx
			}
		}
		if hs != st.HoverSide {
			changed = true
			st.HoverSide = hs
		}
		if changed {
			pInvalidateRect.Call(uintptr(h), 0, 0)
		}
		return 0

	case WM_LBUTTONDOWN:
		x, y := loWord(lp), hiWord(lp)
		switch hitTest(x, y) {
		case 1:
			pShowWindow.Call(uintptr(h), SW_MINIMIZE)
		case 2:
			pShowWindow.Call(uintptr(h), SW_MAXIMIZE)
		case 3:
			pPostMessageW.Call(uintptr(h), WM_CLOSE, 0, 0)
		case 6:
			openSetup()
		case 7:
			doSaveKey()
		case 8:
			closeSetup()
		case 4:
			eng.NewSession()
		case 5:
			if st.Busy {
				eng.Cancel()
			} else {
				doSend()
			}
		default:
			// 点侧栏会话
			if x <= sidebarW && y > rcNewBtn.Bottom+8 {
				idx := int((y - (rcNewBtn.Bottom + 8) + st.SideScroll) / itemH)
				if idx >= 0 && idx < len(st.Sessions) {
					st.CurSession = st.Sessions[idx].ID
					st.Msgs = nil
					eng.LoadMessages(st.CurSession)
					pInvalidateRect.Call(uintptr(h), 0, 0)
				}
			}
		}
		return 0

	case WM_MOUSEWHEEL:
		delta := int32(int16(hiWord(wp)))
		var pt POINT
		pt.X = loWord(lp)
		pt.Y = hiWord(lp)
		pScreenToClient.Call(uintptr(h), uintptr(unsafe.Pointer(&pt)))
		if pt.X <= sidebarW {
			st.SideScroll -= delta / WHEEL_DELTA * itemH
			if st.SideScroll < 0 {
				st.SideScroll = 0
			}
			maxS := int32(len(st.Sessions)*itemH) - (clientRect(h).Bottom - rcNewBtn.Bottom)
			if maxS < 0 {
				maxS = 0
			}
			if st.SideScroll > maxS {
				st.SideScroll = maxS
			}
		} else {
			st.ChatScroll -= delta / WHEEL_DELTA * 60
			if st.ChatScroll < 0 {
				st.ChatScroll = 0
			}
		}
		pInvalidateRect.Call(uintptr(h), 0, 0)
		return 0

	case WM_NCHITTEST:
		// 自绘标题栏：除按钮外都交给系统拖动
		var pt POINT
		pt.X = int32(int16(lp & 0xFFFF))
		pt.Y = int32(int16((lp >> 16) & 0xFFFF))
		pScreenToClient.Call(uintptr(h), uintptr(unsafe.Pointer(&pt)))
		if hitTest(pt.X, pt.Y) == 0 && pt.Y <= titleBarH && pt.Y >= 0 {
			return HTCAPTION
		}
		return HTCLIENT

	case WM_GETMINMAXINFO:
		type MINMAXINFO struct {
			ptReserved     POINT
			ptMaxSize      POINT
			ptMaxPosition  POINT
			ptMinTrackSize POINT
			ptMaxTrackSize POINT
		}
		mmi := (*MINMAXINFO)(unsafe.Pointer(lp))
		mmi.ptMinTrackSize.X = 760
		mmi.ptMinTrackSize.Y = 520
		return 0

	case WM_CTLCOLOREDIT:
		br, _, _ := pCreateSolidBrush.Call(uintptr(colSurface))
		pSetBkColor.Call(wp, uintptr(colSurface))
		pSetTextColor.Call(wp, uintptr(colText))
		return br

	case WM_CLOSE:
		eng.Stop()
		pDestroyWindow.Call(uintptr(h))
		return 0

	case WM_DESTROY:
		releaseFonts()
		pPostQuitMessage.Call(0)
		return 0
	}
	r, _, _ := pDefWindowProcW.Call(uintptr(h), uintptr(msg), wp, lp)
	return r
}

func relayoutChildren() {
	cr := clientRect(hwndMain)
	layout(cr)
	if hEdit != 0 {
		// EDIT 放在输入卡片内，左侧留白，右侧给发送按钮
		pSetWindowPos.Call(uintptr(hEdit), 0,
			uintptr(rcInputCard.Left+10), uintptr(rcInputCard.Top+6),
			uintptr(rcSendBtn.Left-rcInputCard.Left-18), uintptr(rcInputCard.Bottom-rcInputCard.Top-12),
			SWP_NOZORDER|SWP_NOACTIVATE)
	}
}

func main() {
	// 高 DPI
	pSetProcessDPIAware.Call()

	hInst, _, _ := pGetModuleHandleW.Call(0)
	hInstG = hInst

	// 通用控件（EDIT 需要）
	icc := INITCOMMONCONTROLSEX{DwSize: uint32(unsafe.Sizeof(INITCOMMONCONTROLSEX{})), DwICC: 0x0000FFFF}
	pInitCommonControlsEx.Call(uintptr(unsafe.Pointer(&icc)))

	hdcScreen, _, _ := pGetDC.Call(0)
	lpx, _, _ := pGetDeviceCaps.Call(hdcScreen, LOGPIXELSY)
	dpi = int32(lpx)
	pReleaseDC.Call(0, hdcScreen)
	if dpi <= 0 {
		dpi = 96
	}
	createFonts(dpi)

	className := u16("FlareWin32Class")
	wc := WNDCLASSEX{
		CbSize:        uint32(unsafe.Sizeof(WNDCLASSEX{})),
		Style:         CS_HREDRAW | CS_VREDRAW,
		LpfnWndProc:   syscall.NewCallback(wndProc),
		HInstance:     syscall.Handle(hInst),
		HCursor:       mustCursor(IDC_ARROW),
		HbrBackground: 0,
		LpszClassName: className,
	}
	if r, _, _ := pRegisterClassExW.Call(uintptr(unsafe.Pointer(&wc))); r == 0 {
		return
	}

	scrW, _, _ := pGetSystemMetrics.Call(SM_CXSCREEN)
	scrH, _, _ := pGetSystemMetrics.Call(SM_CYSCREEN)
	ww, wh := int32(1040), int32(700)
	if int32(scrW) < ww+80 {
		ww = int32(scrW) - 80
	}
	if int32(scrH) < wh+80 {
		wh = int32(scrH) - 80
	}
	x := (int32(scrW) - ww) / 2
	y := (int32(scrH) - wh) / 2

	hwnd, _, _ := pCreateWindowExW.Call(
		WS_EX_APPWINDOW,
		uintptr(unsafe.Pointer(className)),
		uintptr(unsafe.Pointer(u16("Flare"))),
		WS_POPUP|WS_CLIPCHILDREN|WS_VISIBLE,
		uintptr(x), uintptr(y), uintptr(ww), uintptr(wh),
		0, 0, hInst, 0,
	)
	if hwnd == 0 {
		return
	}
	hwndMain = syscall.Handle(hwnd)

	// 输入框
	hEdit = createInputEdit(hwndMain)

	// 圆角窗口区域（Win7 用 SetWindowRgn 兜底，Win11 走系统圆角）
	relayoutChildren()

	pShowWindow.Call(uintptr(hwndMain), SW_SHOW)
	pUpdateWindow.Call(uintptr(hwndMain))

	// 启动引擎
	eng.hwnd = uintptr(hwndMain)
	if err := eng.Start(); err != nil {
		st.Status = "引擎启动失败：" + err.Error()
	}

	// 首次使用还没有 API Key → 直接进设置页，不让用户面对一个不能说话的窗口
	if readAPIKey() == "" {
		openSetup()
	}

	// 消息循环
	var m MSG
	for {
		r, _, _ := pGetMessageW.Call(uintptr(unsafe.Pointer(&m)), 0, 0, 0)
		if int32(r) <= 0 {
			break
		}
		pTranslateMessage.Call(uintptr(unsafe.Pointer(&m)))
		pDispatchMessageW.Call(uintptr(unsafe.Pointer(&m)))
	}
}

func mustCursor(id uintptr) syscall.Handle {
	h, _, _ := pLoadCursorW.Call(0, id)
	return syscall.Handle(h)
}

func createInputEdit(parent syscall.Handle) syscall.Handle {
	style := uintptr(WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL)
	h, _, _ := pCreateWindowExW.Call(
		0,
		uintptr(unsafe.Pointer(u16("EDIT"))),
		uintptr(unsafe.Pointer(u16(""))),
		style,
		0, 0, 100, 40,
		uintptr(parent), 0, 0, 0,
	)
	he := syscall.Handle(h)
	if he != 0 {
		pSendMessageW.Call(uintptr(he), WM_SETFONT, fontInput, 1)
		// 子类化：Enter 发送，Shift+Enter 换行
		gwlpWndProc := int32(GWLP_WNDPROC)
		oldEditProc, _, _ = pSetWindowLongPtrW.Call(uintptr(he), uintptr(gwlpWndProc),
			syscall.NewCallback(editProc))
	}
	return he
}

func editProc(h syscall.Handle, msg uint32, wp, lp uintptr) uintptr {
	if msg == WM_KEYDOWN && wp == 0x0D { // VK_RETURN
		sh, _, _ := pGetKeyState.Call(0x10) // VK_SHIFT
		if int16(sh) >= 0 {
			doSend()
			return 0
		}
	}
	r, _, _ := pCallWindowProcW.Call(oldEditProc, uintptr(h), uintptr(msg), wp, lp)
	return r
}
