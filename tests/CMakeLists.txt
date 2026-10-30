# 测试子目录（TDD）
# 每个测试文件一个可执行文件，通过 CTest 注册

add_executable(t_framework framework_test.cpp)
target_link_libraries(t_framework PRIVATE Qt6::Test)
add_test(NAME framework COMMAND t_framework)

add_executable(t_mainwindow mainwindow_test.cpp)
target_link_libraries(t_mainwindow PRIVATE Qt6::Test flare_client_core)
add_test(NAME mainwindow COMMAND t_mainwindow)

add_executable(t_engine_bridge engine_bridge_test.cpp)
target_link_libraries(t_engine_bridge PRIVATE Qt6::Test flare_client_core)
add_test(NAME engine_bridge COMMAND t_engine_bridge)

# M1 冒烟（真实 chat 集成验证；不注册进 ctest，手动运行）
add_executable(m1_smoke m1_smoke.cpp)
target_link_libraries(m1_smoke PRIVATE Qt6::Core flare_client_core)

# M15-3：时间戳口径（裸 UTC 串 vs RFC3339，本地时区换算、分组归属）
add_executable(t_flaretime flaretime_test.cpp)
target_link_libraries(t_flaretime PRIVATE Qt6::Test flare_client_core)
add_test(NAME flaretime COMMAND t_flaretime)

add_executable(t_chatwidget chatwidget_test.cpp)
target_link_libraries(t_chatwidget PRIVATE Qt6::Test flare_client_core)
add_test(NAME chatwidget COMMAND t_chatwidget)

# M3-1 会话列表侧边栏
# M15 消息区代码块「复制」按钮
add_executable(t_markdownview markdownview_test.cpp)
target_link_libraries(t_markdownview PRIVATE Qt6::Test flare_client_core)
add_test(NAME markdownview COMMAND t_markdownview)

add_executable(t_sessionlist sessionlistwidget_test.cpp)
target_link_libraries(t_sessionlist PRIVATE Qt6::Test flare_client_core)
add_test(NAME sessionlist COMMAND t_sessionlist)

# M3-2 记忆查看面板
add_executable(t_memorypanel memorypanel_test.cpp)
target_link_libraries(t_memorypanel PRIVATE Qt6::Test flare_client_core)
add_test(NAME memorypanel COMMAND t_memorypanel)

# M6-2 设置面板（.env 读写）
add_executable(t_settingspanel settingspanel_test.cpp)
target_link_libraries(t_settingspanel PRIVATE Qt6::Test flare_client_core)
add_test(NAME settingspanel COMMAND t_settingspanel)

# M8 用户反馈回归（回收站/别名/工作目录/粘贴图片）
add_executable(t_m8_feedback m8_feedback_test.cpp)
target_link_libraries(t_m8_feedback PRIVATE Qt6::Test flare_client_core)
add_test(NAME m8_feedback COMMAND t_m8_feedback)

# GUI 渲染验证（验收截图；手动运行: QT_QPA_PLATFORM=offscreen ./gui_capture）
add_executable(gui_capture gui_capture.cpp)
target_link_libraries(gui_capture PRIVATE Qt6::Widgets flare_client_core)

# M2 端到端验收（真实 UI 自测对话；不注册进 ctest，手动运行）
add_executable(e2e_acceptance e2e_acceptance.cpp)
target_link_libraries(e2e_acceptance PRIVATE Qt6::Widgets flare_client_core)

# 重入安全复现（真实引擎 + 工具调用 + confirm 弹窗自动点击；手动运行）
add_executable(reentrancy_live reentrancy_live.cpp)
target_link_libraries(reentrancy_live PRIVATE Qt6::Widgets flare_client_core)

# M8-1 版本与更新（GUI 版本 + 内核名字/版本，两条检查互相独立）
# MarkdownLite 渲染（表格/代码块/内联标记；纯字符串处理，不需要 QApplication）
add_executable(t_markdownlite markdownlite_test.cpp)
target_link_libraries(t_markdownlite PRIVATE Qt6::Test flare_client_core)
add_test(NAME markdownlite COMMAND t_markdownlite)

# CLI 基本礼仪：--version / --help 必须在没有图形环境时也能用。
# 这条测试特意不设 QT_QPA_PLATFORM —— 它验证的正是「不需要显示器」。
add_test(NAME cli_version COMMAND flare-client --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")
add_test(NAME cli_help COMMAND flare-client --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "--screenshot")

add_executable(t_version version_test.cpp)
target_link_libraries(t_version PRIVATE Qt6::Test flare_client_core)
add_test(NAME version COMMAND t_version)

# 手动视觉验证工具（不注册 ctest）：主窗口状态栏 + 版本面板截图 + 真实更新检查
add_executable(panel_capture panel_capture.cpp)
target_link_libraries(panel_capture PRIVATE Qt6::Widgets flare_client_core)
