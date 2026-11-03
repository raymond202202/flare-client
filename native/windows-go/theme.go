// theme.go —— 火焰主题（flame theme）
//
// 色值全部取自 Fedora Qt 版 src/flametheme.h（Qt 版是你的样板，Win32 版跟随它，
// 不自己发明颜色）：
//
//	FLAME_TOKENS: red #ef4444 / orange #f97316 / amber #f59e0b
//	              / yellow #fbbf24 / dark #b45309
//	火焰主行动色 = orange #f97316；底色系 = 米白 #fffbf0 / 浅琥珀 #fff3d6
//
// 历史教训：本文件一度被写成白底 + 紫 #6D4AFF（那是另一个项目的配色），
// 与 flare 的火焰视觉规范完全不符。改配色前先看 flametheme.h，不要凭印象。
package main

import (
	"strings"
	"syscall"
)

// 颜色（COLORREF = 0x00BBGGRR，用 mkColor 构造）
var (
	colBg        = mkColor(0xFF, 0xFB, 0xF0) // 窗口底色 —— bgSoft 米白
	colSurface   = mkColor(0xFF, 0xFD, 0xF9) // 卡片/主区，兼作橙底上的白字
	colSidebar   = mkColor(0xFF, 0xF3, 0xD6) // 侧栏 —— bgAlt 浅琥珀
	colSidebarHi = mkColor(0xFF, 0xED, 0xD0) // 侧栏选中/悬停 —— hoverSoft
	colBorder    = mkColor(0xFD, 0xE6, 0xBF) // 分隔线 —— borderSoft 浅橙
	colPrimary   = mkColor(0xF9, 0x73, 0x16) // 主色 —— 火焰橙
	colPrimaryDk = mkColor(0xE0, 0x63, 0x0F) // 主色按下 —— 深橙
	colPrimaryBg = mkColor(0xFF, 0xED, 0xD0) // 主色浅底 —— hoverSoft
	colText      = mkColor(0x4A, 0x2E, 0x0D) // 主文本 —— textMain 深琥珀
	colTextDim   = mkColor(0x8A, 0x6A, 0x3B) // 次文本 —— textSoft
	colTextMute  = mkColor(0x9A, 0x8F, 0x80) // 弱文本 —— 暖灰
	colBubble    = mkColor(0xFF, 0xF7, 0xED) // AI 气泡 —— 极浅橙
	colUserBg    = mkColor(0xF9, 0x73, 0x16) // 用户气泡 —— 火焰橙
	colUserText  = mkColor(0xFF, 0xFF, 0xFF)
	colHover     = mkColor(0xFF, 0xED, 0xD0) // 悬停底 —— hoverSoft
	colDanger    = mkColor(0xEF, 0x44, 0x44) // 危险/错误 —— 火焰核心红
	colScroll    = mkColor(0xFD, 0xE6, 0xBF) // 滚动条 —— borderSoft
	colAmber     = mkColor(0xF5, 0x9E, 0x0B) // amber 过渡色（备用）
	colYellow    = mkColor(0xFB, 0xBF, 0x24) // yellow 亮黄（备用）
	colFlameDark = mkColor(0xB4, 0x53, 0x09) // dark 暗琥珀（备用）
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
