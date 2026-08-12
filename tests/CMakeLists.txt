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

add_executable(t_chatwidget chatwidget_test.cpp)
target_link_libraries(t_chatwidget PRIVATE Qt6::Test flare_client_core)
add_test(NAME chatwidget COMMAND t_chatwidget)

# M3-1 会话列表侧边栏
add_executable(t_sessionlist sessionlistwidget_test.cpp)
target_link_libraries(t_sessionlist PRIVATE Qt6::Test flare_client_core)
add_test(NAME sessionlist COMMAND t_sessionlist)

# M3-2 记忆查看面板
add_executable(t_memorypanel memorypanel_test.cpp)
target_link_libraries(t_memorypanel PRIVATE Qt6::Test flare_client_core)
add_test(NAME memorypanel COMMAND t_memorypanel)

# GUI 渲染验证（验收截图；手动运行: QT_QPA_PLATFORM=offscreen ./gui_capture）
add_executable(gui_capture gui_capture.cpp)
target_link_libraries(gui_capture PRIVATE Qt6::Widgets flare_client_core)

# M2 端到端验收（真实 UI 自测对话；不注册进 ctest，手动运行）
add_executable(e2e_acceptance e2e_acceptance.cpp)
target_link_libraries(e2e_acceptance PRIVATE Qt6::Widgets flare_client_core)

# 重入安全复现（真实引擎 + 工具调用 + confirm 弹窗自动点击；手动运行）
add_executable(reentrancy_live reentrancy_live.cpp)
target_link_libraries(reentrancy_live PRIVATE Qt6::Widgets flare_client_core)
