#include <QtTest>
#include <QApplication>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTimer>
#include <QJsonObject>
#include "../src/chatwidget.h"
#include "../src/mainwindow.h"
#include "engine_bridge.h"

// ============================================================
// M2 聊天 UI 测试
// 1) UI 结构：输入框/输出区/发送按钮存在
// 2) 发送动作：输入文字点发送 → 消息进入输出区 + 输入框清空
// 3) 集成：MainWindow 启动时能连接 flare server（真实引擎）
// ============================================================

class ChatWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void widgetHasInputAndOutput();
    void sendAppendsMessage();
    void sendClearsInput();
    void mainWindowConnectsEngine();
};

void ChatWidgetTest::widgetHasInputAndOutput()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QVERIFY(w.input() != nullptr);
    QVERIFY(w.output() != nullptr);
    QVERIFY(w.input()->placeholderText().contains("输入"));
}

void ChatWidgetTest::sendAppendsMessage()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.input()->setText(QStringLiteral("你好 flare"));
    w.sendMessage(); // 直接调 slot（不真发网络，只是 UI 行为）
    QVERIFY(w.outputText().contains("你好 flare"));
}

void ChatWidgetTest::sendClearsInput()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.input()->setText(QStringLiteral("测试消息"));
    w.sendMessage();
    QVERIFY(w.input()->text().isEmpty());
}

void ChatWidgetTest::mainWindowConnectsEngine()
{
    if (!QProcessEnvironment::systemEnvironment().contains("FLARE_AVAILABLE"))
        QSKIP("未设置 FLARE_AVAILABLE，跳过引擎连接测试");
    MainWindow w;
    QVERIFY(w.engine() != nullptr);
    QVERIFY2(w.engine()->isRunning(), "MainWindow 应成功启动 flare server");
}

QTEST_MAIN(ChatWidgetTest)
#include "chatwidget_test.moc"
