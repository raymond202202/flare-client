; ============================================================
; Flare Windows 兼容版安装向导（NSIS 3 / Unicode）
;
; 与 installer.nsi 的区别：
;   标准版（installer.nsi）     = WPF on .NET Framework 4.8，~4MB，靠系统自带 4.8
;   兼容版（installer-compat.nsi）= WPF on .NET 6 自包含运行时，~40MB，把运行时打进包
;
; 为什么需要兼容版：
;   Windows 7 SP1 系统自带 .NET Framework 3.5.1，不满足 4.8；而 .NET 4.8 离线安装包
;   121MB 且需要 SHA-2 补丁链（KB4490628 → KB4474419）+ 管理员 + 重启，在 Win7 上
;   失败率高。.NET 6 自包含把 CoreCLR + WPF + UCRT（ucrtbase.dll、api-ms-win-crt-*）
;   全部随包携带，目标机无需安装任何东西，SP1 即可运行。
;
; 覆盖范围：Windows 7 SP1 / 8 / 8.1 / 10 / 11（x64）
;   注：.NET 6 是最后一个支持 Win7 SP1 的 .NET 版本（已 EOL，无安全更新，
;   但网络通信全部在独立的 Go 引擎进程内，壳只做 UI + 本地 IPC）。
;
; 构建：makensis -INPUTCHARSET UTF8 -DVERSION=0.6.191 -DSRCDIR=/tmp/compatpkg -DOUTDIR=/tmp installer-compat.nsi
; ============================================================

Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
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

Name "${APPNAME}（Win7 兼容版）"
OutFile "${OUTDIR}\flare-${VERSION}-win-x64-compat-setup.exe"
InstallDir "$LOCALAPPDATA\${APPNAME}"
InstallDirRegKey HKCU "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 64

VIProductVersion "${NUMVERSION}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "CompanyName" "${COMPANY}"
VIAddVersionKey "FileDescription" "Flare 安装程序（Win7 兼容版，内置 .NET 运行时）"
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
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM Flare.exe /T'
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM flare.exe /T'
  Sleep 500

  ; 自包含发布会产出 200+ 个文件（CoreCLR、WPF、UCRT、本地化资源），必须整目录递归
  File /r "${SRCDIR}\*"

  ; 安装标记：卸载器据此确认该目录确实由 Flare 安装器创建，避免误删用户自定义目录
  FileOpen $9 "$INSTDIR\.flare-install" w
  FileWrite $9 "Flare ${VERSION} win-x64-compat"
  FileClose $9

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; 注册表（HKCU，不需要管理员）
  WriteRegStr HKCU "Software\${APPNAME}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\${APPNAME}" "Edition" "win-x64-compat"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayName" "${APPNAME}（Win7 兼容版）"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTKEY}" "Publisher" "${COMPANY}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\Flare.exe"
  WriteRegStr HKCU "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1

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
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Flare 主程序、内置引擎与 .NET 运行时（必需）。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "在开始菜单创建 Flare 快捷方式。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "在桌面创建 Flare 快捷方式。"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---------- 卸载段 ----------
Section "Uninstall"
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM Flare.exe /T'
  ExecWait '"$SYSDIR\taskkill.exe" /F /IM flare.exe /T'
  Sleep 400

  ; 安全护栏：只有存在安装标记时才整目录删除，且路径必须含 Flare，
  ; 防止用户把安装目录选到盘符根或用户主目录时被 RMDir /r 清空
  IfFileExists "$INSTDIR\.flare-install" 0 nostrip
    StrCpy $R0 "$INSTDIR" "" -5
    StrCmp $R0 "Flare" 0 nostrip
      RMDir /r "$INSTDIR"
      Goto stripsdone
  nostrip:
    Delete "$INSTDIR\Flare.exe"
    Delete "$INSTDIR\.flare-install"
    Delete "$INSTDIR\Uninstall.exe"
  stripsdone:

  DeleteRegKey HKCU "${UNINSTKEY}"
  DeleteRegKey HKCU "Software\${APPNAME}"

  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\卸载 Flare.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"
  Delete "$DESKTOP\${APPNAME}.lnk"
SectionEnd
