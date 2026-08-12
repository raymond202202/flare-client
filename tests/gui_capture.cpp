// GUI 渲染验证：offscreen 渲染主窗口并保存 PNG（验收截图）
// M3-7 增强：注入 M3-3 流式块 / M3-4 工具卡片演示内容 + 等待 M3-6 模型信息
#include <QApplication>
#include <QTimer>
#include <QDir>
#include <QJsonObject>
#include "mainwindow.h"
#include "chatwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow w;
    w.show();

    QTimer::singleShot(2500, [&]() {
        // 注入演示内容（纯 UI 事件，不依赖真实生成）
        if (auto *chat = w.findChild<ChatWidget *>()) {
            // M3-3 流式：两个 chunk 合并到同一消息块
            chat->onEngineEvent(QJsonObject{
                {"type", "text"}, {"content", "这是流式渲染的第一段…"}});
            chat->onEngineEvent(QJsonObject{
                {"type", "text"}, {"content", "第二段 chunk 增量追加到同一消息块。"}});
            // M3-4 工具调用卡片
            chat->onEngineEvent(QJsonObject{
                {"type", "tool_call"}, {"content", "read_file"}});
            // M3-4 工具结果卡片
            chat->onEngineEvent(QJsonObject{
                {"type", "tool_result"},
                {"toolName", "read_file"},
                {"content", "OK: 读取 24 行 /README.md"}});
            chat->onEngineEvent(QJsonObject{{"type", "done"}});
        }
        // 等待引擎 models 事件刷新状态栏（M3-6）
        QTimer::singleShot(2500, [&]() {
            const QPixmap pm = w.grab();
            const QString path = QDir::homePath() + "/Desktop/flare-gui-preview.png";
            if (!pm.save(path))
                qFatal("保存截图失败");
            qInfo("截图已保存: %s (%dx%d)", qPrintable(path), pm.width(), pm.height());
            qInfo("窗口标题: %s", qPrintable(w.windowTitle()));
            qInfo("引擎连接: %s", w.engine()->isRunning() ? "是" : "否");
            qInfo("模型信息: %s", qPrintable(w.modelInfoText()));
            app.exit(0);
        });
    });

    return app.exec();
}
