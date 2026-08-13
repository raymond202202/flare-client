cmake_minimum_required(VERSION 3.20)
project(flare-client VERSION 0.4.0 LANGUAGES CXX)

# ============================================================
# Flare GUI 客户端 — 三平台统一 Qt 6 Widgets
# Fedora / macOS / Windows
# TDD：CTest + QtTest，测试在 tests/ 目录
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# ---------- Qt 6 依赖 ----------
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

# ---------- 核心库（主程序 + 测试共用）----------
add_library(flare_client_core STATIC
  src/mainwindow.h
  src/mainwindow.cpp
  src/chatwidget.h
  src/chatwidget.cpp
  src/sessionlistwidget.h
  src/sessionlistwidget.cpp
  src/memorypanel.h
  src/memorypanel.cpp
  src/skillpanel.h
  src/skillpanel.cpp
  src/settingspanel.h
  src/settingspanel.cpp
  engine-bridge/engine_bridge.h
  engine-bridge/engine_bridge.cpp
)
target_link_libraries(flare_client_core PUBLIC Qt6::Widgets)
target_include_directories(flare_client_core PUBLIC src engine-bridge)

# ---------- 主程序 ----------
add_executable(flare-client src/main.cpp)
target_link_libraries(flare-client PRIVATE flare_client_core)

# M6-4: 便携安装版图标 —— 构建后把火焰 SVG 复制到可执行文件同目录
# （mainwindow 图标候选含 <appDir>/flare-icon.svg，之前无任何步骤产出该文件，
#   便携版/开发版从未真正加载到自定义图标；RPM 安装版走 /usr/share/icons 路径）
add_custom_command(TARGET flare-client POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/packaging/flare-client.svg
          $<TARGET_FILE_DIR:flare-client>/flare-icon.svg
  COMMENT "复制 flare-icon.svg 到可执行文件目录（便携版窗口图标）")

# Windows 子系统（无控制台窗口）
if(WIN32)
  set_target_properties(flare-client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ---------- 测试（TDD） ----------
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# ---------- 打包 ----------
# 注意：include(CPack) 必须放在所有 CPACK_* 变量之后（文件末尾），
# 否则 CPackConfig.cmake 快照不会包含这些设置（生成器会退回默认 STGZ;TGZ;TZ）。

# 安装规则：可执行文件 + 桌面入口 + 图标
install(TARGETS flare-client RUNTIME DESTINATION bin)
install(FILES packaging/flare-client.desktop DESTINATION share/applications)
install(FILES packaging/flare-client.svg DESTINATION share/icons/hicolor/scalable/apps)

# ---------- 通用 CPack 配置 ----------
set(CPACK_PACKAGE_NAME "flare-client")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Flare AI Agent GUI Client")
set(CPACK_PACKAGE_VENDOR "Flare")
set(CPACK_PACKAGE_CONTACT "flare-client@example.com")
set(CPACK_PACKAGE_DESCRIPTION "Flare AI Agent native GUI client built with Qt 6 Widgets")
set(CPACK_PACKAGE_FILE_NAME "flare-client_${CPACK_PACKAGE_VERSION}")

# 安装前缀（RPM 默认 /usr，符合系统级安装约定）
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# ---------- RPM 生成器 ----------
set(CPACK_GENERATOR "RPM")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
# Qt 6 运行时依赖（rpm 安装时自动检查）
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.5")

# ---------- macOS DMG 支持（macmini 端已实机验证，fedora 侧提供 CMake 配置）----------
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # M7-1: 显式声明 macOS App Bundle 属性（同步 macmini 实机反馈的修复：
  #       缺 MACOSX_BUNDLE 时 CPack DragNDrop 打出的是裸可执行文件而非 .app，
  #       macmini 真机打 dmg 发现无 .app，本地修复后同步回 fedora 源码）
  set_target_properties(flare-client PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Flare Client"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.flare.client"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
  )
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "Flare Client ${CPACK_PACKAGE_VERSION}")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/Applications")
  install(TARGETS flare-client BUNDLE DESTINATION .)
endif()

# ---------- Windows NSIS/zip 支持（CI/交叉编译时启用）----------
if(WIN32)
  set(CPACK_GENERATOR "ZIP")
  install(TARGETS flare-client RUNTIME DESTINATION .)
endif()

# include(CPack) 必须在所有 CPACK_* 变量之后
include(CPack)
