// Package main —— Win32 绑定（纯 syscall，零 CGO，零 .NET，Win7 SP1+ 可跑）
package main

import (
	"syscall"
	"unsafe"
)

var (
	user32   = syscall.NewLazyDLL("user32.dll")
	gdi32    = syscall.NewLazyDLL("gdi32.dll")
	kernel32 = syscall.NewLazyDLL("kernel32.dll")
	comctl32 = syscall.NewLazyDLL("comctl32.dll")
	shell32  = syscall.NewLazyDLL("shell32.dll")
	dwmapi   = syscall.NewLazyDLL("dwmapi.dll")
)

var (
	// user32
	pRegisterClassExW  = user32.NewProc("RegisterClassExW")
	pCreateWindowExW   = user32.NewProc("CreateWindowExW")
	pDefWindowProcW    = user32.NewProc("DefWindowProcW")
	pDestroyWindow     = user32.NewProc("DestroyWindow")
	pShowWindow        = user32.NewProc("ShowWindow")
	pUpdateWindow      = user32.NewProc("UpdateWindow")
	pGetMessageW       = user32.NewProc("GetMessageW")
	pTranslateMessage  = user32.NewProc("TranslateMessage")
	pDispatchMessageW  = user32.NewProc("DispatchMessageW")
	pPostQuitMessage   = user32.NewProc("PostQuitMessage")
	pBeginPaint        = user32.NewProc("BeginPaint")
	pEndPaint          = user32.NewProc("EndPaint")
	pGetClientRect     = user32.NewProc("GetClientRect")
	pInvalidateRect    = user32.NewProc("InvalidateRect")
	pFillRect          = user32.NewProc("FillRect")
	pDrawTextW         = user32.NewProc("DrawTextW")
	pLoadCursorW       = user32.NewProc("LoadCursorW")
	pGetSystemMetrics  = user32.NewProc("GetSystemMetrics")
	pSetCapture        = user32.NewProc("SetCapture")
	pReleaseCapture    = user32.NewProc("ReleaseCapture")
	pSendMessageW      = user32.NewProc("SendMessageW")
	pPostMessageW      = user32.NewProc("PostMessageW")
	pSetWindowPos      = user32.NewProc("SetWindowPos")
	pGetWindowRect     = user32.NewProc("GetWindowRect")
	pSetFocus          = user32.NewProc("SetFocus")
	pGetKeyState       = user32.NewProc("GetKeyState")
	pMessageBoxW       = user32.NewProc("MessageBoxW")
	pSetTimer          = user32.NewProc("SetTimer")
	pKillTimer         = user32.NewProc("KillTimer")
	pSystemParametersInfoW = user32.NewProc("SystemParametersInfoW")
	pGetDC             = user32.NewProc("GetDC")
	pReleaseDC         = user32.NewProc("ReleaseDC")
	pScreenToClient    = user32.NewProc("ScreenToClient")
	pSetCursor         = user32.NewProc("SetCursor")
	pShowScrollBar     = user32.NewProc("ShowScrollBar")
	pSetWindowTextW    = user32.NewProc("SetWindowTextW")
	pGetWindowTextW    = user32.NewProc("GetWindowTextW")
	pGetWindowTextLengthW = user32.NewProc("GetWindowTextLengthW")
	pSetWindowLongPtrW = user32.NewProc("SetWindowLongPtrW")
	pGetWindowLongPtrW = user32.NewProc("GetWindowLongPtrW")
	pCallWindowProcW   = user32.NewProc("CallWindowProcW")
	pGetCursorPos      = user32.NewProc("GetCursorPos")
	pOpenClipboard     = user32.NewProc("OpenClipboard")
	pCloseClipboard    = user32.NewProc("CloseClipboard")
	pEnableWindow      = user32.NewProc("EnableWindow")
	pSetForegroundWindow = user32.NewProc("SetForegroundWindow")
	pMonitorFromPoint  = user32.NewProc("MonitorFromPoint")
	pGetMonitorInfoW   = user32.NewProc("GetMonitorInfoW")
	pRegisterHotKey    = user32.NewProc("RegisterHotKey")
	pSetProcessDPIAware = user32.NewProc("SetProcessDPIAware")
	pSetWindowRgn      = user32.NewProc("SetWindowRgn")
	pCreateRoundRectRgn = gdi32.NewProc("CreateRoundRectRgn")

	// gdi32
	pCreateSolidBrush   = gdi32.NewProc("CreateSolidBrush")
	pCreatePen          = gdi32.NewProc("CreatePen")
	pCreateFontW        = gdi32.NewProc("CreateFontW")
	pSelectObject       = gdi32.NewProc("SelectObject")
	pDeleteObject       = gdi32.NewProc("DeleteObject")
	pSetBkMode          = gdi32.NewProc("SetBkMode")
	pSetTextColor       = gdi32.NewProc("SetTextColor")
	pSetBkColor         = gdi32.NewProc("SetBkColor")
	pTextOutW           = gdi32.NewProc("TextOutW")
	pRoundRect          = gdi32.NewProc("RoundRect")
	pRectangle          = gdi32.NewProc("Rectangle")
	pMoveToEx           = gdi32.NewProc("MoveToEx")
	pLineTo             = gdi32.NewProc("LineTo")
	pCreateCompatibleDC = gdi32.NewProc("CreateCompatibleDC")
	pCreateCompatibleBitmap = gdi32.NewProc("CreateCompatibleBitmap")
	pBitBlt             = gdi32.NewProc("BitBlt")
	pDeleteDC           = gdi32.NewProc("DeleteDC")
	pGetTextExtentPoint32W = gdi32.NewProc("GetTextExtentPoint32W")
	pGetStockObject     = gdi32.NewProc("GetStockObject")
	pGetDeviceCaps      = gdi32.NewProc("GetDeviceCaps")
	pGetTextFaceW       = gdi32.NewProc("GetTextFaceW")

	// kernel32
	pGetModuleHandleW = kernel32.NewProc("GetModuleHandleW")
	pCreateProcessW   = kernel32.NewProc("CreateProcessW")
	pCreatePipe       = kernel32.NewProc("CreatePipe")
	pSetHandleInformation = kernel32.NewProc("SetHandleInformation")
	pPeekNamedPipe    = kernel32.NewProc("PeekNamedPipe")
	pReadFile         = kernel32.NewProc("ReadFile")
	pWriteFile        = kernel32.NewProc("WriteFile")
	pCloseHandle      = kernel32.NewProc("CloseHandle")
	pGetLastError     = kernel32.NewProc("GetLastError")
	pTerminateProcess = kernel32.NewProc("TerminateProcess")
	pWaitForSingleObject = kernel32.NewProc("WaitForSingleObject")

	// shell32
	pShellExecuteW = shell32.NewProc("ShellExecuteW")

	// comctl32
	pInitCommonControlsEx = comctl32.NewProc("InitCommonControlsEx")
)

// ---- Win32 常量 ----
const (
	WS_OVERLAPPED   = 0x00000000
	WS_POPUP        = 0x80000000
	WS_CHILD        = 0x40000000
	WS_BORDER       = 0x00800000
	WS_VISIBLE      = 0x10000000
	WS_CLIPCHILDREN = 0x02000000
	WS_CLIPSIBLINGS = 0x04000000
	WS_TABSTOP      = 0x00010000
	WS_VSCROLL      = 0x00200000
	WS_EX_APPWINDOW = 0x00040000
	WS_EX_TOOLWINDOW = 0x00000080
	WS_EX_NOACTIVATE = 0x08000000
	WS_EX_TOPMOST   = 0x00000008

	WM_DESTROY      = 0x0002
	WM_SIZE         = 0x0005
	WM_ACTIVATE     = 0x0006
	WM_PAINT        = 0x000F
	WM_CLOSE        = 0x0010
	WM_QUIT         = 0x0012
	WM_KEYDOWN      = 0x0100
	WM_CHAR         = 0x0102
	WM_TIMER        = 0x0113
	WM_MOUSEMOVE    = 0x0200
	WM_LBUTTONDOWN  = 0x0201
	WM_LBUTTONUP    = 0x0202
	WM_RBUTTONUP    = 0x0205
	WM_MOUSEWHEEL   = 0x020A
	WM_GETMINMAXINFO = 0x0024
	WM_ERASEBKGND   = 0x0014
	WM_NCCALCSIZE   = 0x0083
	WM_NCHITTEST    = 0x0084
	WM_APP          = 0x8000
	WM_NCLBUTTONDOWN = 0x00A1
	WM_COMMAND      = 0x0111
	WM_SETFONT      = 0x0030
	WM_CTLCOLOREDIT = 0x0133
	WM_SETTEXT      = 0x000C
	WM_GETTEXT      = 0x000D
	WM_GETTEXTLENGTH = 0x000E

	HTCAPTION = 2
	HTCLIENT  = 1

	SW_SHOW     = 5
	SW_HIDE     = 0
	SW_RESTORE  = 9
	SW_MINIMIZE = 6
	SW_MAXIMIZE = 3

	CS_HREDRAW = 0x0002
	CS_VREDRAW = 0x0001
	CS_DBLCLKS = 0x0008

	IDC_ARROW = 32512

	COLOR_WINDOW = 5

	TRANSPARENT = 1
	OPAQUE      = 2

	DT_LEFT     = 0x00000000
	DT_CENTER   = 0x00000001
	DT_RIGHT    = 0x00000002
	DT_TOP      = 0x00000000
	DT_VCENTER  = 0x00000004
	DT_BOTTOM   = 0x00000008
	DT_WORDBREAK = 0x00000010
	DT_SINGLELINE = 0x00000020
	DT_NOPREFIX = 0x00000800
	DT_END_ELLIPSIS = 0x00008000
	DT_CALCRECT = 0x00000400

	SRCCOPY = 0x00CC0020

	FW_NORMAL = 400
	FW_SEMIBOLD = 600
	FW_BOLD   = 700

	DEFAULT_CHARSET = 1
	CLEARTYPE_QUALITY = 5
	ANTIALIASED_QUALITY = 4

	SWP_NOSIZE     = 0x0001
	SWP_NOMOVE     = 0x0002
	SWP_NOZORDER   = 0x0004
	SWP_NOACTIVATE = 0x0010
	SWP_FRAMECHANGED = 0x0020

	IDC_SIZENS = 32645
	IDC_SIZEWE = 32644
	IDC_HAND   = 32649

	GWLP_USERDATA = -21
	GWLP_WNDPROC  = -4

	SM_CXSCREEN = 0
	SM_CYSCREEN = 1
	SM_CXFULLSCREEN = 16
	SM_CYFULLSCREEN = 17

	ES_MULTILINE  = 0x0004
	ES_AUTOVSCROLL = 0x0040
	ES_WANTRETURN = 0x1000
	ES_NOHIDESEL  = 0x0100
	ES_SAVESEL    = 0x8000

	MB_OK = 0x0
	MB_ICONERROR = 0x10
	MB_ICONINFORMATION = 0x40

	WHEEL_DELTA = 120

	PS_SOLID   = 0
	NULL_BRUSH = 5
	NULL_PEN   = 8
	LOGPIXELSY = 88
)

// ---- 结构体 ----
type POINT struct{ X, Y int32 }

type RECT struct{ Left, Top, Right, Bottom int32 }

func (r RECT) W() int32 { return r.Right - r.Left }
func (r RECT) H() int32 { return r.Bottom - r.Top }

type MSG struct {
	Hwnd    syscall.Handle
	Message uint32
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	Pt      POINT
	Private uint32
}

type PAINTSTRUCT struct {
	Hdc         syscall.Handle
	FErase      int32
	RcPaint     RECT
	FRestore    int32
	FIncUpdate  int32
	RgbReserved [32]byte
}

type WNDCLASSEX struct {
	CbSize        uint32
	Style         uint32
	LpfnWndProc   uintptr
	CbClsExtra    int32
	CbWndExtra    int32
	HInstance     syscall.Handle
	HIcon         syscall.Handle
	HCursor       syscall.Handle
	HbrBackground syscall.Handle
	LpszMenuName  *uint16
	LpszClassName *uint16
	HIconSm       syscall.Handle
}

type INITCOMMONCONTROLSEX struct {
	DwSize uint32
	DwICC  uint32
}

type STARTUPINFO struct {
	Cb            uint32
	LpReserved    *uint16
	LpDesktop     *uint16
	LpTitle       *uint16
	DwX           uint32
	DwY           uint32
	DwXSize       uint32
	DwYSize       uint32
	DwXCountChars uint32
	DwYCountChars uint32
	DwFillAttribute uint32
	DwFlags       uint32
	WShowWindow   uint16
	CbReserved2   uint16
	LpReserved2   *byte
	HStdInput     syscall.Handle
	HStdOutput    syscall.Handle
	HStdError     syscall.Handle
}

type PROCESS_INFORMATION struct {
	HProcess    syscall.Handle
	HThread     syscall.Handle
	DwProcessId uint32
	DwThreadId  uint32
}

type SIZE struct{ CX, CY int32 }

type MONITORINFO struct {
	CbSize    uint32
	RcMonitor RECT
	RcWork    RECT
	DwFlags   uint32
}

// ---- 辅助 ----
func u16(s string) *uint16 {
	p, err := syscall.UTF16PtrFromString(s)
	if err != nil {
		return nil
	}
	return p
}

func unsafePtr(p *uint16) unsafe.Pointer { return unsafe.Pointer(p) }

func loWord(v uintptr) int32  { return int32(v & 0xFFFF) }
func hiWord(v uintptr) int32  { return int32((v >> 16) & 0xFFFF) }
func mkColor(r, g, b byte) uint32 { return uint32(r) | uint32(g)<<8 | uint32(b)<<16 }

func rgb(c uint32) (byte, byte, byte) {
	return byte(c & 0xFF), byte((c >> 8) & 0xFF), byte((c >> 16) & 0xFF)
}
