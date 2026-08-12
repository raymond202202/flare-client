# ============================================================
# Windows 构建脚本 — flare-client
# 依赖: Visual Studio 2022 (MSVC) + Qt 6 (MSVC 2022 套件)
#   Qt 安装时勾选 Qt 6.x MSVC 2022 64-bit 组件
# 用法:
#   powershell -ExecutionPolicy Bypass -File packaging/build-windows.ps1
#   powershell -ExecutionPolicy Bypass -File packaging/build-windows.ps1 -Package
# ============================================================
param(
    [string]$QtPath = "C:\Qt\6.x.x\msvc2022_64",
    [switch]$Package
)

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

Write-Host "==> 配置 CMake (Release)"
cmake -B build-windows -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$QtPath

Write-Host "==> 构建"
cmake --build build-windows --config Release

Write-Host "==> 运行测试 (TDD)"
ctest --test-dir build-windows -C Release --output-on-failure

# 打包（可选：-Package 参数触发 windeployqt + CPack zip）
if ($Package) {
    Write-Host "==> windeployqt 收集 Qt 运行库"
    & "$QtPath\bin\windeployqt.exe" --release --no-translations `
        build-windows\Release\flare-client.exe

    Write-Host "==> 打包 ZIP (CPack)"
    cmake --build build-windows --config Release --target package
    if (-not $?) { throw "CPack 打包失败" }

    Write-Host "==> 收集产物到 packaging/dist/"
    New-Item -ItemType Directory -Force -Path packaging/dist | Out-Null
    Get-ChildItem build-windows -Filter "flare-client_*.zip" -File |
        ForEach-Object { Copy-Item $_.FullName "packaging/dist\" -Force }

    Write-Host "==> 打包后自动清理（每类保留最新 3 个）"
    & (Join-Path $PSScriptRoot "cleanup-old-builds.sh")  # 需 WSL/Git Bash 环境
    # 若在纯 PowerShell 环境（无 bash），改用下方等价逻辑：
    # Get-ChildItem packaging/dist -Filter "flare-client_*.zip" -File |
    #   Sort-Object LastWriteTime -Descending | Select-Object -Skip 3 |
    #   Remove-Item -Force

    Write-Host "==> ZIP 产物:"
    Get-ChildItem packaging/dist\flare-client_*.zip | Select-Object FullName, Length
}

Write-Host "==> 产物: build-windows/Release/flare-client.exe"
Get-ChildItem build-windows/Release/flare-client.exe | Select-Object FullName, Length
