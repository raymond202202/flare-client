; ============================================================
; Flare Windows 安装向导（NSIS 3 / Unicode）
;
; 设计原则（用户要求「把用户当傻子」）：
;   · 零依赖：不需要 .NET 8 Desktop Runtime、不需要 Node
;     壳是 net452 编译的：Win7 SP1 只要系统里有任意 .NET 4.5.2+ 就能跑
;     （Win10/11 自带 4.6+，Win7 上过 Windows Update 的基本都有 4.5.2/4.6.2/4.7.2）
;     以前是 net48，会在 Win7 上弹「需要 .NET Framework v4.8」，现在不会了
;   · 零提权：装到 %LOCALAPPDATA%\Flare，RequestExecutionLevel user，不弹 UAC
;   · 零配置：装完直接能从开始菜单/桌面双击启动，引擎已内置在 engine\flare.exe
;     引擎含 Win7 随机数回退补丁（bcryptprimitives.dll 缺失时走 advapi32），Win7 不崩
;   · 可干净卸载：只写 HKCU，卸载器删目录与快捷方式，不留残渣
;
; 构建：makensis -INPUTCHARSET UTF8 -DVERSION=0.6.163 -DSRCDIR=dist\pkg installer.nsi
; ============================================================

Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
; VIProductVersion 只认 3~4 段纯数字，而 VERSION 可能是 0.0.0-dev.<sha>。
; 数字版本由 CI 单独以 -DNUMVERSION 传入；本地手工打包时回落到 VERSION。
!ifndef NUMVERSION
  !define NUMVERSION "${VERSION}"
!endif
!ifndef SRCDIR
  !define SRCDIR "."
!endif
!ifndef OUTDIR
  !define OUTDIR "."
!endif

!define APPNAME "Flare"
!define COMPANY "Flare"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Flare"

Name "${APPNAME}"
OutFile "${OUTDIR}\flare-${VERSION}-win-x64-setup.exe"
InstallDir "$LOCALAPPDATA\${APPNAME}"
InstallDirRegKey HKCU "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 64

VIProductVersion "${NUMVERSION}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "CompanyName" "${COMPANY}"
VIAddVersionKey "FileDescription" "Flare 安装程序"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" ""

; ---------- 界面 ----------
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; 完成页：可直接勾选启动
!define MUI_FINISHPAGE_RUN "$INSTDIR\Flare.exe"
!define MUI_FINISHPAGE_RUN_TEXT "立即启动 Flare"
!define MUI_FINISHPAGE_LINK "使用说明（README）"
!define MUI_FINISHPAGE_LINK_LOCATION "$INSTDIR\README.txt"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; ---------- 安装段 ----------
Section "Flare（必需）" SecCore
  SectionIn RO
  SetOutPath "$INSTDIR"
  SetOverwrite try

  ; 覆盖升级前先结束在跑的客户端与内置引擎，否则文件被占用，SetOverwrite try 会静默跳过
  ; 导致新旧文件混装（版本号对不上、引擎不更新）。静默安装同样需要，故不加提示。
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM Flare.exe /T'
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM flare.exe /T'
  Sleep 500

  File "${SRCDIR}\Flare.exe"
  File "${SRCDIR}\Flare.exe.config"
  File "${SRCDIR}\Newtonsoft.Json.dll"
  File "${SRCDIR}\README.txt"

  SetOutPath "$INSTDIR\engine"
  File "${SRCDIR}\engine\flare.exe"

  ; 卸载器
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; 注册表（HKCU，不需要管理员）
  WriteRegStr HKCU "Software\${APPNAME}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTKEY}" "Publisher" "${COMPANY}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\Flare.exe"
  WriteRegStr HKCU "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1

  ; 估算安装体积，供「应用和功能」显示
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINSTKEY}" "EstimatedSize" "$0"
SectionEnd

Section "开始菜单快捷方式" SecStartMenu
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortCut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\Flare.exe" "" "$INSTDIR\Flare.exe" 0
  CreateShortCut "$SMPROGRAMS\${APPNAME}\卸载 Flare.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "桌面快捷方式" SecDesktop
  CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\Flare.exe" "" "$INSTDIR\Flare.exe" 0
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Flare 主程序与内置引擎（必需）。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "在开始菜单创建 Flare 快捷方式。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "在桌面创建 Flare 快捷方式。"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---------- 卸载段 ----------
Section "Uninstall"
  ; 结束可能残留的进程，避免文件占用导致卸载不全
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM Flare.exe /T'
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM flare.exe /T'
  Sleep 400

  Delete "$INSTDIR\Flare.exe"
  Delete "$INSTDIR\Flare.exe.config"
  Delete "$INSTDIR\Newtonsoft.Json.dll"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\engine\flare.exe"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR\engine"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\卸载 Flare.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"
  Delete "$DESKTOP\${APPNAME}.lnk"

  DeleteRegKey HKCU "${UNINSTKEY}"
  DeleteRegKey HKCU "Software\${APPNAME}"

  ; 询问是否一并删除用户数据（%USERPROFILE%\.flare，含 API Key 与会话记录）
  ; 静默卸载（/S）不弹窗，默认保留用户数据——静默模式下 MessageBox 会永久阻塞且无人可点，
  ; 且「保留」比「误删 API Key 与会话」安全得多。
  IfSilent KeepData
  MessageBox MB_YESNO|MB_ICONQUESTION "是否同时删除你的配置与会话记录？$\r$\n位置：$PROFILE\.flare$\r$\n$\r$\n选择「否」可保留，重装后配置仍在。" IDNO KeepData
    RMDir /r "$PROFILE\.flare"
  KeepData:
SectionEnd

Function .onInit
  ; 已安装则提示升级，直接覆盖安装即可
  ;
  ; 注意：NSIS 的 MessageBox 在 /S 静默模式下【依然会弹出并永久阻塞】——没有窗口可点，
  ; 进程就一直挂在那里，WriteUninstaller 和注册表写入全都执行不到。
  ; 表现为「机器上装过旧版 → 静默升级必卡死 → 目录里只有半套文件、无 Uninstall.exe」。
  ; 因此静默模式必须显式跳过提示，直接覆盖安装。
  IfSilent silent_ok
  ReadRegStr $0 HKCU "${UNINSTKEY}" "DisplayVersion"
  StrCmp $0 "" silent_ok
    MessageBox MB_OKCANCEL|MB_ICONINFORMATION "已检测到 Flare $0 已安装。$\r$\n继续将覆盖安装为 ${VERSION}，你的配置不会被删除。" IDOK silent_ok
    Abort
  silent_ok:
FunctionEnd
