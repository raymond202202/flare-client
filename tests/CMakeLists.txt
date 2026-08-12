# 测试子目录（TDD）
# 每个测试文件一个可执行文件，通过 CTest 注册

add_executable(t_framework framework_test.cpp)
target_link_libraries(t_framework PRIVATE Qt6::Test)
add_test(NAME framework COMMAND t_framework)

add_executable(t_mainwindow mainwindow_test.cpp)
target_link_libraries(t_mainwindow PRIVATE Qt6::Test flare_client_core)
add_test(NAME mainwindow COMMAND t_mainwindow)
