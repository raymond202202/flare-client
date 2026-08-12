# Flare Client — AI Agent 桌面客户端

Flare 是通用能力 AI Agent（TypeScript CLI）的三平台原生 GUI 客户端。

- **技术栈**：Qt 6 Widgets（C++17 / CMake），三平台统一
- **平台**：Fedora Linux / macOS / Windows
- **架构**：客户端与 flare 主项目（CLI）解耦，通过 JSON Lines 子进程协议通信
- **开发方法**：TDD（CTest + QtTest），每个里程碑有验收清单

## 目录结构

```
flare-client/
├── src/            # Qt 主程序（聊天 UI / 设置）
├── engine-bridge/  # QProcess 包装：spawn flare server + JSON Lines 解析（M1）
├── tests/          # TDD 测试（QtTest）
├── packaging/      # 三平台构建脚本
└── docs/           # 开发文档
```

## 构建

### Fedora

```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel
./packaging/build-fedora.sh
```

### macOS

```bash
xcode-select --install
brew install qt@6
./packaging/build-macos.sh
```

### Windows

```powershell
powershell -ExecutionPolicy Bypass -File packaging/build-windows.ps1
```

## 测试

```bash
ctest --test-dir build-fedora --output-on-failure
```

## 协作

项目进展记录在飞书协作文档（fedora-hermes 与 macmini-hermes 共同维护）。
macmini-hermes 每小时读文档检测更新，在 macOS 端跟进编译/打包/冒烟。
