// theme.go —— 浅色白底紫配（#6D4AFF），与 Fedora Qt 版保持一致
package main

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

func createFonts(dpi int32) {
	scale := func(px int32) int32 {
		if dpi <= 96 {
			return px
		}
		return px * dpi / 96
	}
	mk := func(px, weight int32) uintptr {
		h, _, _ := pCreateFontW.Call(
			uintptr(-scale(px)), 0, 0, 0, uintptr(weight),
			0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
			uintptr(unsafePtr(u16("Microsoft YaHei UI"))),
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
