# ============================================================
# Windows 构建脚本 — flare-client
# 依赖: Visual Studio 2022 (MSVC) + Qt 6 (MSVC 2022 套件)
#   Qt 安装时勾选 Qt 6.x MSVC 2022 64-bit 组件
# 用法: powershell -ExecutionPolicy Bypass -File packaging/build-windows.ps1
# ============================================================
param(
    [string]$QtPath = "C:\Qt\6.x.x\msvc2022_64"
)

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

Write-Host "==> 配置 CMake (Release)"
cmake -B build-windows -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$QtPath

Write-Host "==> 构建"
cmake --build build-windows --config Release

Write-Host "==> 运行测试 (TDD)"
ctest --test-dir build-windows -C Release --output-on-failure

Write-Host "==> 产物: build-windows/Release/flare-client.exe"
Get-ChildItem build-windows/Release/flare-client.exe | Select-Object FullName, Length
