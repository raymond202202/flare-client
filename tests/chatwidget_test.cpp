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
    void textChunksMergeIntoOneBlock();
    void doneClosesStream();
    void toolCallClosesStream();
    void userMessageClosesStream();
    void toolCallShowsCard();
    void toolResultShowsCard();
    void respondConfirmSendsAllow();
    void respondConfirmSendsDeny();
};

// 构造一个 text 事件
static QJsonObject textEvent(const QString &content)
{
    return QJsonObject{{"type", "text"}, {"content", content}};
}

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

// M3-3: 多个 text 事件 chunk 应合并到同一个「Flare：」块（增量追加），不重复标签
void ChatWidgetTest::textChunksMergeIntoOneBlock()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.onEngineEvent(textEvent(QStringLiteral("你好")));
    w.onEngineEvent(textEvent(QStringLiteral("，Flare")));
    w.onEngineEvent(textEvent(QStringLiteral("！")));

    const QString out = w.outputText();
    QVERIFY2(out.contains(QStringLiteral("Flare：你好，Flare！")), qPrintable(out));
    QCOMPARE(out.count(QStringLiteral("Flare：")), 1); // 标签只出现一次 → 同一消息块
}

// M3-3: done 事件结束流，后续 text 事件另起新块
void ChatWidgetTest::doneClosesStream()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.onEngineEvent(textEvent(QStringLiteral("第一段")));
    w.onEngineEvent(QJsonObject{{"type", "done"}});
    w.onEngineEvent(textEvent(QStringLiteral("第二段")));

    const QString out = w.outputText();
    QCOMPARE(out.count(QStringLiteral("Flare：")), 2); // 两个独立消息块
}

// M3-3: tool_call 事件应关闭当前流，工具卡片另起块，后续 text 再开新块
void ChatWidgetTest::toolCallClosesStream()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.onEngineEvent(textEvent(QStringLiteral("我来调用工具")));
    w.onEngineEvent(QJsonObject{{"type", "tool_call"}, {"content", "search_web"}});
    w.onEngineEvent(textEvent(QStringLiteral("结果如下")));

    const QString out = w.outputText();
    QCOMPARE(out.count(QStringLiteral("Flare：")), 2);
    QVERIFY2(out.contains(QStringLiteral("🔧 调用工具")), qPrintable(out));
}

// M3-3: 用户发送新消息前自动结束未完成流，chunk 不串到用户消息块
void ChatWidgetTest::userMessageClosesStream()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.onEngineEvent(textEvent(QStringLiteral("回复中")));
    w.input()->setText(QStringLiteral("打断"));
    w.sendMessage();
    w.onEngineEvent(textEvent(QStringLiteral("流已结束后的回复")));

    const QString out = w.outputText();
    QCOMPARE(out.count(QStringLiteral("Flare：")), 2);
    QVERIFY2(out.contains(QStringLiteral("回复中")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("流已结束后的回复")), qPrintable(out));
}

// M3-4: tool_call 事件 → 卡片（🔧 调用工具 + 工具名）
void ChatWidgetTest::toolCallShowsCard()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.onEngineEvent(QJsonObject{{"type", "tool_call"}, {"content", "search_web"}});

    const QString out = w.outputText();
    QVERIFY2(out.contains(QStringLiteral("🔧 调用工具")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("search_web")), qPrintable(out));
}

// M3-4: tool_result 事件 → 卡片（📦 工具名 + 结果内容）
void ChatWidgetTest::toolResultShowsCard()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.onEngineEvent(QJsonObject{
        {"type", "tool_result"},
        {"toolName", "search_web"},
        {"content", "找到 3 条结果"}});

    const QString out = w.outputText();
    QVERIFY2(out.contains(QStringLiteral("📦 search_web")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("找到 3 条结果")), qPrintable(out));
}

// M3-5: respondConfirm(allow_once) → confirm_result 请求（带 sessionId/id/decision）
void ChatWidgetTest::respondConfirmSendsAllow()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QSignalSpy spy(&bridge, &EngineBridge::requestSent);

    w.respondConfirm(QStringLiteral("cid-1"), QStringLiteral("allow_once"));
    QCOMPARE(spy.count(), 1);
    const QJsonObject req = spy.at(0).at(0).toJsonObject();
    QCOMPARE(req.value("type").toString(), QString("confirm_result"));
    QCOMPARE(req.value("sessionId").toString(), w.sessionId());
    QCOMPARE(req.value("id").toString(), QString("cid-1"));
    QCOMPARE(req.value("decision").toString(), QString("allow_once"));
}

// M3-5: respondConfirm(deny) → confirm_result 拒绝
void ChatWidgetTest::respondConfirmSendsDeny()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QSignalSpy spy(&bridge, &EngineBridge::requestSent);

    w.respondConfirm(QStringLiteral("cid-2"), QStringLiteral("deny"));
    QCOMPARE(spy.count(), 1);
    const QJsonObject req = spy.at(0).at(0).toJsonObject();
    QCOMPARE(req.value("type").toString(), QString("confirm_result"));
    QCOMPARE(req.value("id").toString(), QString("cid-2"));
    QCOMPARE(req.value("decision").toString(), QString("deny"));
}

QTEST_MAIN(ChatWidgetTest)
#include "chatwidget_test.moc"
