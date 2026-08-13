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
    void stopButtonShowsDuringStream();
    void stopButtonHidesAfterDone();
    void stopGenerationSendsCancel();
    // M6-4 体验打磨新测试
    void foreignSessionTextIgnored();
    void foreignSessionDoneIgnored();
    void cancelledClosesStream();
    void setSessionCancelsActiveStream();
    void emptyHistoryShowsWelcome();
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

// M4-5: 流式 text 事件到达时停止按钮显示
void ChatWidgetTest::stopButtonShowsDuringStream()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.show(); // 需要真实可见状态才能断言 isVisible()

    QVERIFY(!w.stopButton()->isVisible());
    w.onEngineEvent(textEvent(QStringLiteral("开始回复")));
    QVERIFY2(w.stopButton()->isVisible(), "流式输出中停止按钮应显示");
}

// M4-5: done 事件后停止按钮隐藏
void ChatWidgetTest::stopButtonHidesAfterDone()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.show();

    w.onEngineEvent(textEvent(QStringLiteral("回复中")));
    QVERIFY(w.stopButton()->isVisible());
    w.onEngineEvent(QJsonObject{{"type", "done"}});
    QVERIFY2(!w.stopButton()->isVisible(), "done 后停止按钮应隐藏");
}

// M4-5: stopGeneration() 在流式中发 cancel 协议（sessionId + type）
void ChatWidgetTest::stopGenerationSendsCancel()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.show();
    QSignalSpy spy(&bridge, &EngineBridge::requestSent);

    w.onEngineEvent(textEvent(QStringLiteral("回复中")));
    QVERIFY(w.stopButton()->isVisible());
    w.stopGeneration();
    QCOMPARE(spy.count(), 1);
    const QJsonObject req = spy.at(0).at(0).toJsonObject();
    QCOMPARE(req.value("type").toString(), QString("cancel"));
    QCOMPARE(req.value("sessionId").toString(), w.sessionId());
    QVERIFY2(!w.stopButton()->isVisible(), "停止后按钮应隐藏");
    QVERIFY2(w.outputText().contains(QStringLiteral("已请求停止")), qPrintable(w.outputText()));
}

// M6-4: 串会话防护 —— 引擎所有会话事件都带 sessionId；旧会话（非当前）
// 的 text 事件不得渲染到新会话消息区
void ChatWidgetTest::foreignSessionTextIgnored()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.setSession(QStringLiteral("session-B"));

    // 会话 A（旧）的流式事件：应被忽略
    w.onEngineEvent(QJsonObject{{"type", "text"},
                                {"content", "旧会话的回复"},
                                {"sessionId", "session-A"}});
    QVERIFY2(!w.outputText().contains(QStringLiteral("旧会话的回复")),
             "旧会话的 text 不应渲染到新会话");

    // 当前会话的事件：正常渲染
    w.onEngineEvent(QJsonObject{{"type", "text"},
                                {"content", "当前会话回复"},
                                {"sessionId", "session-B"}});
    QVERIFY2(w.outputText().contains(QStringLiteral("当前会话回复")),
             qPrintable(w.outputText()));
}

// M6-4: 串会话防护 —— 旧会话的 done 事件不得结束新会话的流状态
void ChatWidgetTest::foreignSessionDoneIgnored()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.show();
    w.setSession(QStringLiteral("session-B"));

    // 当前会话开始流式
    w.onEngineEvent(QJsonObject{{"type", "text"},
                                {"content", "回复中"},
                                {"sessionId", "session-B"}});
    QVERIFY(w.stopButton()->isVisible());

    // 旧会话 done 到达：不应隐藏停止按钮/结束当前流
    w.onEngineEvent(QJsonObject{{"type", "done"}, {"sessionId", "session-A"}});
    QVERIFY2(w.stopButton()->isVisible(), "旧会话 done 不应结束当前流");

    // 当前会话 done：正常结束
    w.onEngineEvent(QJsonObject{{"type", "done"}, {"sessionId", "session-B"}});
    QVERIFY(!w.stopButton()->isVisible());
}

// M6-4: cancelled 事件（停止生成生效）→ 关闭流 + 收起提示
void ChatWidgetTest::cancelledClosesStream()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.show();

    w.onEngineEvent(QJsonObject{{"type", "text"}, {"content", "回复中"}});
    QVERIFY(w.stopButton()->isVisible());

    w.onEngineEvent(QJsonObject{{"type", "cancelled"}});
    QVERIFY2(!w.stopButton()->isVisible(), "cancelled 后停止按钮应隐藏");
    QVERIFY2(w.outputText().contains(QStringLiteral("已停止生成")), qPrintable(w.outputText()));
}

// M6-4: 切换会话时若旧会话仍在流式生成，先发 cancel 协议（针对旧会话）
void ChatWidgetTest::setSessionCancelsActiveStream()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QSignalSpy spy(&bridge, &EngineBridge::requestSent);

    const QString oldSession = w.sessionId();
    w.show(); // isVisible() 断言需要真实可见状态
    w.onEngineEvent(QJsonObject{{"type", "text"}, {"content", "回复中"}});
    QVERIFY(w.stopButton()->isVisible());

    w.setSession(QStringLiteral("session-B"));
    // 切换会话会发 2 个请求：先 cancel 旧会话流，再 getMessages 加载新会话历史
    QVERIFY2(spy.count() >= 2, "应发送 cancel + getMessages");
    const QJsonObject req = spy.at(0).at(0).toJsonObject();
    QCOMPARE(req.value("type").toString(), QString("cancel"));
    // cancel 必须针对仍在流式的旧会话，而不是新会话
    QCOMPARE(req.value("sessionId").toString(), oldSession);
    // 第二个请求是 getMessages（新会话）
    const QJsonObject req2 = spy.at(1).at(0).toJsonObject();
    QCOMPARE(req2.value("type").toString(), QString("get_messages"));
    QCOMPARE(req2.value("sessionId").toString(), QString("session-B"));
}

// M6-4: 空会话（无历史）→ 展示跃动欢迎词（用户要求每个会话都展示）
void ChatWidgetTest::emptyHistoryShowsWelcome()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.setSession(QStringLiteral("session-B"));

    w.onEngineEvent(QJsonObject{{"type", "messages"}, {"messages", QJsonArray()}});
    QVERIFY2(w.outputText().contains(QStringLiteral("F L A R E")), qPrintable(w.outputText()));
    QVERIFY2(w.outputText().contains(QStringLiteral("Let your inspiration flare")),
             qPrintable(w.outputText()));
}

QTEST_MAIN(ChatWidgetTest)
#include "chatwidget_test.moc"
