#include <QtTest>
#include <QApplication>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>
#include <QImage>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
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
    void suggestChipsGuideFirstRun();
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
    // M9 打磨
    void markdownRenderedAfterStream();
    void markdownUnclosedCodeBlockStillShows();
    // M10 打磨：拖放附件 + 引擎故障可见性
    void dropAttachesLocalFile();
    void dropIgnoresRemoteUrlAndMissingFile();
    void engineCrashShowsNotice();
    void engineCleanExitStaysQuiet();
    void dropDirectoryRequestsWorkdir();
    void dropMixedFileAndDirectory();
    void pasteImageBecomesAttachment();
    void draftIsolatedPerSession();
    void scrollFollowsOnlyAtBottom();
    void newContentHintWhenScrolledUp();
    void atLatestPositionGuardsLayoutJitter();
};

// 构造一个 text 事件
static QJsonObject textEvent(const QString &content)
{
    return QJsonObject{{"type", "text"}, {"content", content}};
}

// M10：拖放落地的入口就是 attachLocalFiles，直接调它等价于「松手」
void ChatWidgetTest::dropAttachesLocalFile()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QVERIFY(w.attachments().isEmpty());

    // 拿测试自己的二进制当被拖进来的文件，省得造临时文件
    const QUrl url = QUrl::fromLocalFile(QCoreApplication::applicationFilePath());
    w.attachLocalFiles(QList<QUrl>{url});

    QCOMPARE(w.attachments().size(), 1);
    QCOMPARE(w.attachments().first(), QCoreApplication::applicationFilePath());
}

// 拖网页链接 / 拖已删掉的文件都不该挂成附件（否则发送时才炸）
void ChatWidgetTest::dropIgnoresRemoteUrlAndMissingFile()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.attachLocalFiles(QList<QUrl>{QUrl(QStringLiteral("https://example.com/a.png")),
                                   QUrl::fromLocalFile(QStringLiteral("/nonexistent/flare-drop-test.bin"))});

    QVERIFY2(w.attachments().isEmpty(), "非本地文件与不存在的路径都不应进入附件列表");
}

// 文件夹拖进窗口 → 不能当附件（引擎读不出目录内容），要请宿主切工作目录
void ChatWidgetTest::dropDirectoryRequestsWorkdir()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QMimeData md;
    md.setUrls({QUrl::fromLocalFile(tmp.path())});
    QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &md, Qt::LeftButton,
                          Qt::NoModifier);
    QDropEvent ev(QPointF(10, 10), Qt::CopyAction, &md, Qt::LeftButton, Qt::NoModifier);

    QSignalSpy spy(&w, &ChatWidget::directoryDropRequested);
    QCoreApplication::sendEvent(&w, &enter);
    QCoreApplication::sendEvent(&w, &ev);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), tmp.path());
    QCOMPARE(w.attachments().size(), 0); // 目录绝不能混进附件
}

// 文件和文件夹一起拖：文件照常挂附件，文件夹单独抛给宿主
void ChatWidgetTest::dropMixedFileAndDirectory()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QMimeData md;
    md.setUrls({QUrl::fromLocalFile(QCoreApplication::applicationFilePath()),
                QUrl::fromLocalFile(tmp.path())});
    QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &md, Qt::LeftButton,
                          Qt::NoModifier);
    QDropEvent ev(QPointF(10, 10), Qt::CopyAction, &md, Qt::LeftButton, Qt::NoModifier);

    QSignalSpy spy(&w, &ChatWidget::directoryDropRequested);
    QCoreApplication::sendEvent(&w, &enter);
    QCoreApplication::sendEvent(&w, &ev);

    QCOMPARE(w.attachments().size(), 1);
    QCOMPARE(spy.count(), 1);
}

// 截图/网页图片复制后按 Ctrl+V：必须挂成附件，不能被 QLineEdit 当文本吞掉
void ChatWidgetTest::pasteImageBecomesAttachment()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    QImage img(4, 4, QImage::Format_RGB32);
    img.fill(Qt::red);
    QApplication::clipboard()->setImage(img);
    const QMimeData *md = QApplication::clipboard()->mimeData();
    if (!md || !md->hasImage())
        QSKIP("当前平台剪贴板不支持图片，跳过");

    QLineEdit *in = w.findChild<QLineEdit *>(QStringLiteral("chatInput"));
    QVERIFY(in);
    const int before = w.attachments().size();
    QTest::keySequence(in, QKeySequence::Paste);

    QCOMPARE(w.attachments().size(), before + 1);
    const QString path = w.attachments().last();
    QVERIFY2(QFileInfo::exists(path), qPrintable(path));
    QVERIFY(path.endsWith(QStringLiteral(".png")));
}

// 每个会话各自留存草稿：切走再切回来字和附件都还在，也不会串到别的会话被误发
void ChatWidgetTest::draftIsolatedPerSession()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QLineEdit *in = w.findChild<QLineEdit *>(QStringLiteral("chatInput"));
    QVERIFY(in);

    w.setSession(QStringLiteral("sess-a"));
    in->setText(QStringLiteral("A 写到一半的想法"));
    w.attachLocalFiles({QUrl::fromLocalFile(QCoreApplication::applicationFilePath())});
    QCOMPARE(w.attachments().size(), 1);

    w.setSession(QStringLiteral("sess-b"));
    QVERIFY2(in->text().isEmpty(), "换了会话，上一会话没发出去的字不能跟过来");
    QVERIFY(w.attachments().isEmpty());
    in->setText(QStringLiteral("B 的内容"));

    w.setSession(QStringLiteral("sess-a"));
    QCOMPARE(in->text(), QStringLiteral("A 写到一半的想法"));
    QCOMPARE(w.attachments().size(), 1);

    w.setSession(QStringLiteral("sess-b"));
    QCOMPARE(in->text(), QStringLiteral("B 的内容"));
}

// 用户往上翻读旧消息时不能被新内容拽回底部；本来就贴着底部才自动跟随
void ChatWidgetTest::scrollFollowsOnlyAtBottom()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.resize(640, 360);
    w.show();
    QTextEdit *out = w.findChild<QTextEdit *>(QStringLiteral("chatOutput"));
    QVERIFY(out);

    QString bulk;
    for (int i = 0; i < 80; ++i)
        bulk += QStringLiteral("填充行 %1 —— 用来把滚动条撑出来\n").arg(i);
    w.onEngineEvent(textEvent(bulk));
    QTest::qWait(50);
    QScrollBar *bar = out->verticalScrollBar();
    if (bar->maximum() <= 0)
        QSKIP("离屏平台未产生滚动范围，跳过");

    QCOMPARE(bar->value(), bar->maximum());   // 追加消息后应停在底部

    bar->setValue(0);                          // 用户手动翻到最上面
    QTest::qWait(20);
    w.onEngineEvent(textEvent(QStringLiteral("流式期间又来了一段")));
    QTest::qWait(20);
    QCOMPARE(bar->value(), 0);                 // 没有被拽回底部

    bar->setValue(bar->maximum());             // 用户自己滚回底部
    QTest::qWait(20);
    w.onEngineEvent(textEvent(QStringLiteral("回到最新")));
    QTest::qWait(20);
    QCOMPARE(bar->value(), bar->maximum());    // 重新开始跟随
}

// 用户上翻读历史时新内容到达：不能把人拽回底部，也不能什么都不提示
void ChatWidgetTest::newContentHintWhenScrolledUp()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.resize(640, 360);
    w.show();
    QTextEdit *out = w.findChild<QTextEdit *>(QStringLiteral("chatOutput"));
    QVERIFY(out);
    QPushButton *hint = w.findChild<QPushButton *>(QStringLiteral("jumpToLatestButton"));
    QVERIFY2(hint, "上翻时新内容到达必须有可点的跳转提示");
    QVERIFY2(hint->isHidden(), "没有新内容时不该有浮标挡在对话上");

    QString bulk;
    for (int i = 0; i < 80; ++i)
        bulk += QStringLiteral("填充行 %1 —— 用来把滚动条撑出来\n").arg(i);
    w.onEngineEvent(textEvent(bulk));
    QTest::qWait(50);
    QScrollBar *bar = out->verticalScrollBar();
    if (bar->maximum() <= 0)
        QSKIP("离屏平台未产生滚动范围，跳过");

    bar->setValue(0); // 用户翻到最上面读旧消息
    QTest::qWait(20);
    w.onEngineEvent(textEvent(QStringLiteral("上翻期间到达的新内容")));
    QTest::qWait(20);
    QCOMPARE(bar->value(), 0); // 位置没被抢走
    QVERIFY2(!hint->isHidden(), "上翻读历史时有新内容，必须浮出提示，否则用户以为卡死了");

    // 用户自己滚回底部 → 提示要自动消失。
    // 离屏平台有时算不出 QTextEdit 的滚动范围（maximum 停在 0），那种情况下这段没法可靠验证。
    if (bar->maximum() <= 0)
        QSKIP("离屏平台未算出滚动范围，跳过「回到底部自动收起」的界面断言");
    bar->setValue(bar->maximum());
    QTest::qWait(60);
    QVERIFY2(hint->isHidden(), "用户自己回到最新后提示要自动消失");
}

// 「该不该收起提示」的判定：范围没算出来时绝不能当成已经回到底部
void ChatWidgetTest::atLatestPositionGuardsLayoutJitter()
{
    QVERIFY2(ChatWidget::atLatestPosition(1372, 1372), "贴底就是最新位置");
    QVERIFY2(ChatWidget::atLatestPosition(1360, 1372), "48px 容差内也算在最新位置");
    QVERIFY2(!ChatWidget::atLatestPosition(0, 1372), "还在上翻不算最新位置");
    QVERIFY2(!ChatWidget::atLatestPosition(0, 0), "范围没算出来时不能当成回到底部");
}

// 引擎跑到一半崩掉，必须在对话区说出来
void ChatWidgetTest::engineCrashShowsNotice()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    emit bridge.processExited(2);

    QVERIFY2(w.outputText().contains(QStringLiteral("引擎异常退出")),
             "引擎非 0 退出码必须在对话区可见，否则用户只看到『发出去了没人回』");
    QVERIFY(w.outputText().contains(QStringLiteral("2")));
}

// 我们主动 stop() 时退出码 0，不该往对话区塞噪音
void ChatWidgetTest::engineCleanExitStaysQuiet()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    emit bridge.processExited(0);

    QVERIFY2(!w.outputText().contains(QStringLiteral("引擎异常退出")),
             "正常停止不应该报警");
}

void ChatWidgetTest::widgetHasInputAndOutput()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QVERIFY(w.input() != nullptr);
    QVERIFY(w.output() != nullptr);
    QVERIFY(w.input()->placeholderText().contains("输入"));
}

// 首启引导：欢迎海报下面必须有「可以点这个开始」的起点，
// 而且要「只填不发」+ 一开始对话就收起（引导不是常驻噪音）。
void ChatWidgetTest::suggestChipsGuideFirstRun()
{
    EngineBridge bridge;
    ChatWidget widget(&bridge);
    QPushButton *chip = nullptr;
    for (auto *b : widget.findChildren<QPushButton *>()) {
        if (b->text().contains(QStringLiteral("表格")))
            chip = b;
    }
    QVERIFY2(chip, "首启应有示例起点 chip");
    QVERIFY2(widget.suggestVisible(), "欢迎态下引导必须可见");
    QCOMPARE(widget.input()->text(), QString());   // 初始不该被预填

    chip->click();
    QCOMPARE(widget.input()->text(), chip->text()); // 点一下只填进输入框…
    QVERIFY2(!widget.output()->toPlainText().contains(chip->text()), "点 chip 只填不发");

    widget.dismissWelcome();
    QVERIFY2(!widget.suggestVisible(), "开始对话后引导必须收起");
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


// ---- M9: 流式结束后把 Markdown 渲染成富文本（不再让用户看到 ** 和 ```）----
void ChatWidgetTest::markdownRenderedAfterStream()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.onEngineEvent(textEvent(QStringLiteral(
        "**要点**\n- 第一条\n- 第二条\n行内 `config.yaml` 已改\n```cpp\nint a = 1;\n```")));
    w.onEngineEvent(QJsonObject{{"type", "done"}});

    const QString out = w.outputText();
    QVERIFY2(!out.contains(QStringLiteral("**")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("```")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("`config.yaml`")), qPrintable(out)); // 反引号被吃掉
    QVERIFY2(out.contains(QStringLiteral("要点")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("第一条")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("config.yaml")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("int a = 1;")), qPrintable(out));
    // 「Flare：」标签只出现一次，说明渲染没有把消息块拆开
    QCOMPARE(out.count(QStringLiteral("Flare：")), 1);
}

// 模型偶尔忘记收尾的代码块也要显示出来，不能整段吞掉
void ChatWidgetTest::markdownUnclosedCodeBlockStillShows()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    w.onEngineEvent(textEvent(QStringLiteral("看这里\n```bash\nmake all")));
    w.onEngineEvent(QJsonObject{{"type", "done"}});

    const QString out = w.outputText();
    QVERIFY2(out.contains(QStringLiteral("make all")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("```")), qPrintable(out));
}

QTEST_MAIN(ChatWidgetTest)
#include "chatwidget_test.moc"
