// theme.go —— 浅色白底紫配（#6D4AFF），与 Fedora Qt 版保持一致
package main

import (
	"strings"
	"syscall"
)

// 颜色（COLORREF = 0x00BBGGRR，用 mkColor 构造）
var (
	colBg        = mkColor(0xFA, 0xFA, 0xFC) // 窗口底色
	colSurface   = mkColor(0xFF, 0xFF, 0xFF) // 卡片/主区
	colSidebar   = mkColor(0xF5, 0xF4, 0xFA) // 侧栏
	colSidebarHi = mkColor(0xFF, 0xFF, 0xFF) // 侧栏选中项
	colBorder    = mkColor(0xE9, 0xE7, 0xF2) // 分隔线
	colPrimary   = mkColor(0x6D, 0x4A, 0xFF) // 主色
	colPrimaryDk = mkColor(0x5A, 0x38, 0xE0) // 主色按下
	colPrimaryBg = mkColor(0xEF, 0xEB, 0xFF) // 主色浅底
	colText      = mkColor(0x1F, 0x1F, 0x2E) // 主文本
	colTextDim   = mkColor(0x6B, 0x6B, 0x80) // 次文本
	colTextMute  = mkColor(0xA0, 0xA0, 0xB4) // 弱文本
	colBubble    = mkColor(0xF4, 0xF3, 0xFA) // AI 气泡
	colUserBg    = mkColor(0x6D, 0x4A, 0xFF) // 用户气泡
	colUserText  = mkColor(0xFF, 0xFF, 0xFF)
	colHover     = mkColor(0xEE, 0xEC, 0xF8)
	colDanger    = mkColor(0xE5, 0x48, 0x4D)
	colScroll    = mkColor(0xD6, 0xD3, 0xE4)
)

// 布局
const (
	titleBarH  = 48  // 自绘标题栏
	sidebarW   = 248 // 侧栏宽
	inputH     = 96  // 底部输入区
	pad        = 16
	radius     = 10
	itemH      = 56 // 侧栏会话项高
	btnW       = 32 // 标题栏按钮宽
)

// 字体
var (
	fontUI     uintptr // 14px 常规
	fontUIBold uintptr // 14px 半粗
	fontTitle  uintptr // 15px 半粗
	fontSmall  uintptr // 12px 常规
	fontInput  uintptr // 15px 常规
)

var (
	chosenFont string   // 实际生效的界面字体名（探测结果）
	fontProbe  []string // 探测过程：每个候选 → 系统实际给的字体
)

// pickUIFontName 挑一个系统里真实存在的中文字体。
//
// 为什么必须回读：CreateFontW 遇到不存在的字体名不会报错，而是静默换成相近字体，
// 所以只能新建一个字体选进 DC，再用 GetTextFaceW 问系统"你实际给我的是哪个"。
// 不这么做的话，Win7 上写死 "Microsoft YaHei UI"（Win8 才有）就会退化成方框或糊字。
//
// 顺序照顾老系统：YaHei UI（Win8+）→ YaHei（Win7 自带）→ 黑体 → 宋体 → 英文兜底。
func pickUIFontName() string {
	cands := []string{
		"Microsoft YaHei UI", // Win8 / Win10 / Win11
		"Microsoft YaHei",    // Win7 自带雅黑
		"SimHei",             // 黑体，所有中文 Windows 都有
		"SimSun",             // 宋体，最后的简体中文兜底
		"Tahoma",             // 纯英文环境
	}
	dc, _, _ := pCreateCompatibleDC.Call(0)
	if dc == 0 {
		return cands[0]
	}
	defer pDeleteDC.Call(dc)

	buf := make([]uint16, 96)
	negH := int32(-14) // 变量而非常量，否则不能转 uintptr
	for _, name := range cands {
		h, _, _ := pCreateFontW.Call(
			uintptr(negH), 0, 0, 0, uintptr(FW_NORMAL),
			0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
			uintptr(unsafePtr(u16(name))),
		)
		if h == 0 {
			continue
		}
		old, _, _ := pSelectObject.Call(dc, h)
		n, _, _ := pGetTextFaceW.Call(dc, uintptr(len(buf)), uintptr(unsafePtr(&buf[0])))
		pSelectObject.Call(dc, old)
		pDeleteObject.Call(h)
		got := ""
		if n > 1 {
			got = strings.TrimSpace(syscall.UTF16ToString(buf[:n-1]))
		}
		fontProbe = append(fontProbe, name+" => "+got)
		if strings.EqualFold(got, name) {
			return name
		}
	}
	return cands[0]
}

func createFonts(dpi int32) {
	scale := func(px int32) int32 {
		if dpi <= 96 {
			return px
		}
		return px * dpi / 96
	}
	chosenFont = pickUIFontName()
	mk := func(px, weight int32) uintptr {
		h, _, _ := pCreateFontW.Call(
			uintptr(-scale(px)), 0, 0, 0, uintptr(weight),
			0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
			uintptr(unsafePtr(u16(chosenFont))),
		)
		return h
	}
	fontUI = mk(14, FW_NORMAL)
	fontUIBold = mk(14, FW_SEMIBOLD)
	fontTitle = mk(15, FW_SEMIBOLD)
	fontSmall = mk(12, FW_NORMAL)
	fontInput = mk(15, FW_NORMAL)
}

func releaseFonts() {
	for _, f := range []uintptr{fontUI, fontUIBold, fontTitle, fontSmall, fontInput} {
		if f != 0 {
			pDeleteObject.Call(f)
		}
	}
}
