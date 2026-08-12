cmake_minimum_required(VERSION 3.20)
project(flare-client VERSION 0.1.0 LANGUAGES CXX)

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
)
target_link_libraries(flare_client_core PUBLIC Qt6::Widgets)
target_include_directories(flare_client_core PUBLIC src)

# ---------- 主程序 ----------
add_executable(flare-client src/main.cpp)
target_link_libraries(flare-client PRIVATE flare_client_core)

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
include(CPack)
set(CPACK_PACKAGE_NAME "flare-client")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Flare AI Agent GUI Client")
