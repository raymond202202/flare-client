// GUI 渲染验证：offscreen 渲染主窗口并保存 PNG（验收截图）
// M3-7 增强：注入 M3-3 流式块 / M3-4 工具卡片演示内容 + 等待 M3-6 模型信息
#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QDir>
#include <QJsonObject>
#include "mainwindow.h"
#include "chatwidget.h"
#include "sessionlistwidget.h"
#include <QDateTime>
#include <QJsonArray>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow w;
    w.show();

    QTimer::singleShot(2500, [&]() {
        // 注入演示内容（纯 UI 事件，不依赖真实生成）
        // M13: 注入三条不同日期会话，截图时能看到「今天 / 昨天 / 更早」分组标题
        if (auto *sessions = w.findChild<SessionListWidget *>()) {
            const QDateTime now = QDateTime::currentDateTime();
            const auto iso = [](const QDateTime &dt) {
                return dt.toUTC().toString(Qt::ISODate);
            };
            QJsonArray arr;
            arr.append(QJsonObject{{"id", "demo-today"}, {"title", "今天调的主题色"},
                                   {"updatedAt", iso(now)}});
            arr.append(QJsonObject{{"id", "demo-yest"}, {"title", "昨天问的表格渲染"},
                                   {"updatedAt", iso(now.addDays(-1))}});
            arr.append(QJsonObject{{"id", "demo-old"}, {"title", "上个月的引擎迁移"},
                                   {"updatedAt", iso(now.addDays(-40))}});
            // 跨年会话：验证「更早」档里带年份显示（只写 03-15 分不清哪一年）
            arr.append(QJsonObject{{"id", "demo-lastyear"}, {"title", "去年今天的对话"},
                                   {"updatedAt", iso(now.addYears(-1))}});
            sessions->onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});
        }

        auto *chat = w.findChild<ChatWidget *>();
        qInfo("找到 ChatWidget: %s", chat ? "是" : "否");
        if (chat) {
            // M3-3 流式：多个 chunk 合并到同一消息块（M9: 收流后应渲染成富文本）
            chat->onEngineEvent(QJsonObject{
                {"type", "text"}, {"content", "这是流式渲染的第一段…"}});
            chat->onEngineEvent(QJsonObject{
                {"type", "text"}, {"content", "第二段 chunk 增量追加到同一消息块。\n"}});
            chat->onEngineEvent(QJsonObject{
                {"type", "text"}, {"content", "**结论**：把 `config.yaml` 里的 model 改成 deepseek 即可。\n"
                                              "- 第一件事\n- 第二件事"}});
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
        // M9: 注入后立刻处理事件并截屏（中间若引擎回空历史会把演示内容覆盖成欢迎页）
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        w.repaint();
        [&]() {
            const QPixmap pm = w.grab();
            // 输出路径可传参（默认仍落桌面，方便手动看）
            const QString path = argc > 1
                ? QString::fromLocal8Bit(argv[1])
                : QDir::homePath() + "/Desktop/flare-gui-preview.png";
            if (!pm.save(path))
                qFatal("保存截图失败");
            qInfo("截图已保存: %s (%dx%d)", qPrintable(path), pm.width(), pm.height());
            qInfo("窗口标题: %s", qPrintable(w.windowTitle()));
            qInfo("引擎连接: %s", w.engine()->isRunning() ? "是" : "否");
            qInfo("模型信息: %s", qPrintable(w.modelInfoText()));
            app.exit(0);
        }();
    });

    return app.exec();
}
