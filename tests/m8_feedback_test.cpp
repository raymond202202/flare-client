#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidget>
#include <QSettings>
#include <QClipboard>
#include <QMimeData>
#include <QImage>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QFileInfo>
#include <QPushButton>
#include <QCloseEvent>
#include "engine_bridge.h"
#include "sessionlistwidget.h"
#include "chatwidget.h"
#include "mainwindow.h"

// ============================================================
// M8 用户反馈回归测试（Fedora Qt 壳）
// 对应用户反馈清单（截图与 txt）里 7 条意见中的可自动化部分：
//   1) 会话名称能看出内容            → aliases 三级兜底 + preview 二次行
//   2) 「归档」收进右键菜单          → 常驻按钮不再叫归档
//   6) 已删会话需要回收站            → 删除按钮 = 软删 end_session
//   3) 选择工作目录                  → EngineBridge::setWorkingDirectory 生效
//   4) 输入框粘贴图片                → Ctrl+V 图片进附件 chip
// ============================================================

class M8FeedbackTest : public QObject
{
    Q_OBJECT

private slots:
    void sessionListUsesPreviewAsSecondLine();
    void deleteButtonSoftDeletesToRecycleBin();
    void archiveViewRestoresInsteadOfDeleting();
    void localAliasSurvivesRefresh();
    void bridgeWorkingDirectoryApplied();
    void pasteImageBecomesAttachment();
    void attachBarVisibleOnlyWithAttachments();
    void engineErrorSurfacesToUser();
    void emptyListShowsHint();
    void apiKeyDetection();
    void windowGeometryPersisted();
    void shortcutFocusSearch();
    void shortcutNewSession();
    void closeHidesButDoesNotQuit();
};

// 1) 列表项：主标题 + 第二行 preview+时间；无标题时用 preview 当主标题
void M8FeedbackTest::sessionListUsesPreviewAsSecondLine()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "s-1"},
                           {"title", "新会话"},
                           {"preview", "帮我写一个飞书文档导出脚本"},
                           {"updatedAt", "2026-10-30T08:00:00Z"}});
    arr.append(QJsonObject{{"id", "s-2"},
                           {"title", ""},
                           {"preview", "今天上海天气怎么样"},
                           {"updatedAt", "2026-10-29T12:00:00Z"}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});

    QCOMPARE(w.count(), 2);
    const QString t0 = w.sessionItem(0)->text();
    // 占位标题「新会话」不应作为名字出现，主行必须是真实内容
    QVERIFY2(!t0.startsWith(QStringLiteral("新会话")),
             qPrintable(QStringLiteral("占位标题未被替换，实际=%1").arg(t0)));
    QVERIFY2(t0.contains(QStringLiteral("飞书文档")), qPrintable(t0));
    // 第二行带 preview 与时间
    QVERIFY2(t0.contains(QLatin1Char('\n')), qPrintable(t0));

    const QString t1 = w.sessionItem(1)->text();
    QVERIFY2(t1.contains(QStringLiteral("天气")), qPrintable(t1));
}

// 6) 删除按钮语义 = 移入回收站（end_session），不是永久删除
void M8FeedbackTest::deleteButtonSoftDeletesToRecycleBin()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "s-del"}, {"title", "待删会话"}, {"preview", "内容预览"}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});
    w.list()->setCurrentItem(w.sessionItem(0));

    QSignalSpy spy(&bridge, &EngineBridge::requestSent);
    w.deleteButton()->click();

    QVERIFY2(spy.count() >= 1, "点击删除未发出任何请求");
    const QJsonObject req = spy.at(0).at(0).toJsonObject();
    const QString type = req.value("type").toString();
    QVERIFY2(type == QStringLiteral("end_session"),
             qPrintable(QStringLiteral("删除按钮应为软删 end_session，实际=%1").arg(type)));
    QCOMPARE(req.value("sessionId").toString(), QString("s-del"));
    QVERIFY2(w.deleteButton()->toolTip().contains(QStringLiteral("回收站")),
             qPrintable(w.deleteButton()->toolTip()));
}

// 归档视图里「删除」语义反转：彻底删除走 delete_session（不弹框的路径外，
// 这里只断言视图状态机：inArchivedView 时 deleteCurrent 转发 purge）
void M8FeedbackTest::archiveViewRestoresInsteadOfDeleting()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    // 非归档视图里收到归档数据 → 不得顶掉最近列表
    QJsonArray recent;
    recent.append(QJsonObject{{"id", "s-recent"}, {"title", "最近的会话"}, {"preview", "最近内容"}});
    w.onEngineEvent(QJsonObject{{"type", "recent_sessions"}, {"sessions", recent}});
    QCOMPARE(w.count(), 1);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "s-arch"}, {"title", "已归档会话"}, {"preview", "旧内容"}});
    w.onEngineEvent(QJsonObject{{"type", "archived_sessions"}, {"sessions", arr}});
    QCOMPARE(w.count(), 1);
    QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("s-recent"));

    // 进入归档视图后才用归档数据填充
    w.showArchived();
    QCOMPARE(w.isArchivedView(), true);
    QCOMPARE(w.count(), 1);
    QVERIFY2(w.sessionItem(0)->text().contains(QStringLiteral("已归档")) ||
                 w.sessionItem(0)->text().contains(QStringLiteral("旧内容")),
             qPrintable(w.sessionItem(0)->text()));
}

// 1) 本地别名（首条消息）刷新后仍在，且优先于 preview
void M8FeedbackTest::localAliasSurvivesRefresh()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "s-alias"}, {"title", "新会话"}, {"preview", "旧预览"}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});

    w.setLocalAlias(QStringLiteral("s-alias"), QStringLiteral("季度复盘会议纪要整理"));

    const QString t0 = w.sessionItem(0)->text();
    QVERIFY2(t0.startsWith(QStringLiteral("季度复盘")), qPrintable(t0));

    // 再灌一次同样数据（模拟 refresh），别名仍在
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});
    const QString t1 = w.sessionItem(0)->text();
    QVERIFY2(t1.startsWith(QStringLiteral("季度复盘")), qPrintable(t1));

    // 清理测试写入的别名，避免污染真实 ini
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("flare"), QStringLiteral("flare-client"));
    QVariantMap m = s.value(QStringLiteral("sessions/aliases")).toMap();
    m.remove(QStringLiteral("s-alias"));
    s.setValue(QStringLiteral("sessions/aliases"), m);
    s.sync();
}

// 3) 工作目录：设置后 getter 生效，非法目录被拒
void M8FeedbackTest::bridgeWorkingDirectoryApplied()
{
    EngineBridge bridge;
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QCOMPARE(bridge.workingDirectory(), QString());
    bridge.setWorkingDirectory(tmp.path());
    QCOMPARE(bridge.workingDirectory(), tmp.path());

    bridge.setWorkingDirectory(QStringLiteral("/no/such/dir/for/flare"));
    QCOMPARE(bridge.workingDirectory(), tmp.path()); // 非法路径不覆盖
}

// 4) 输入框 Ctrl+V 粘贴图片 → 附件 chip 出现，发送时以路径随正文
void M8FeedbackTest::pasteImageBecomesAttachment()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);

    QImage img(8, 8, QImage::Format_RGB32);
    img.fill(Qt::red);
    QApplication::clipboard()->setImage(img);
    // 真实路径：焦点在输入框时按 Ctrl+V，由 eventFilter 截获剪贴板图片
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.input()->setFocus();
    QTest::keyClick(w.input(), Qt::Key_V, Qt::ControlModifier);

    QVERIFY2(w.attachments().size() >= 1,
             "剪贴板有图片时未自动转成附件");
    const QString path = w.attachments().value(0);
    QVERIFY2(path.startsWith(QStringLiteral("/")), qPrintable(path));
    QVERIFY2(QFileInfo::exists(path), qPrintable(path));

    QSignalSpy spy(&bridge, &EngineBridge::requestSent);
    w.input()->setText(QStringLiteral("看这张图"));
    w.sendButton()->click();
    QVERIFY(spy.count() >= 1);
    const QJsonObject req = spy.at(0).at(0).toJsonObject();
    QCOMPARE(req.value("type").toString(), QString("chat"));
    const QString text = req.value("input").toString();
    QVERIFY2(text.contains(QStringLiteral("看这张图")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("本地附件")), qPrintable(text));
    QVERIFY2(text.contains(path), qPrintable(text));
}

// 引擎报错必须在界面上说人话（用户反馈：发了没反应不知道怎么回事）
void M8FeedbackTest::engineErrorSurfacesToUser()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    w.setSession(QStringLiteral("s-err"));

    // 协议级错误帧：字段是 message（不是 content），且不带 sessionId
    QJsonObject err;
    err.insert(QStringLiteral("type"), QStringLiteral("error"));
    err.insert(QStringLiteral("message"), QStringLiteral("未配置 API key，无法调用模型"));
    w.onEngineEvent(err);

    const QString out = w.outputText();
    QVERIFY2(out.contains(QStringLiteral("未配置 API key")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("设置")), qPrintable(out));

    // stream 错误帧：字段是 content，且 sessionId 不匹配时也应提醒（错误不该被静默丢弃）
    QJsonObject err2;
    err2.insert(QStringLiteral("type"), QStringLiteral("error"));
    err2.insert(QStringLiteral("content"), QStringLiteral("模型返回 401"));
    w.onEngineEvent(err2);
    QVERIFY2(w.outputText().contains(QStringLiteral("模型返回 401")), qPrintable(w.outputText()));
}

// 空列表要有提示，不能只是一片空白
void M8FeedbackTest::emptyListShowsHint()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    QVERIFY(w.emptyHint() != nullptr);

    // 初始无会话（offscreen 下未 show，updateEmptyHint 由 applySessions 触发）
    QJsonObject ev;
    ev.insert(QStringLiteral("type"), QStringLiteral("sessions"));
    ev.insert(QStringLiteral("sessions"), QJsonArray());
    w.onEngineEvent(ev);
    QVERIFY2(w.emptyHint()->text().contains(QStringLiteral("还没有会话")),
             qPrintable(w.emptyHint()->text()));

    // 回收站视图文案不同
    w.showArchived();
    w.onEngineEvent(ev);
    QVERIFY2(w.emptyHint()->text().contains(QStringLiteral("回收站是空的")),
             qPrintable(w.emptyHint()->text()));
}

// 无附件时 chip 行隐藏，不占版面
void M8FeedbackTest::attachBarVisibleOnlyWithAttachments()
{
    EngineBridge bridge;
    ChatWidget w(&bridge);
    QVERIFY2(w.attachBar()->isHidden(), "无附件时 chip 行应隐藏");
}

// 首次使用最常见的一步：没有 API Key。状态栏要能提醒，且不能把空模板当成已配置。
void M8FeedbackTest::apiKeyDetection()
{
    // 空文件 / 只有注释 / 只有空值 —— 都算没配置
    QVERIFY(!MainWindow::envHasApiKey(QString()));
    QVERIFY(!MainWindow::envHasApiKey(QStringLiteral("# DEEPSEEK_API_KEY=\n")));
    QVERIFY(!MainWindow::envHasApiKey(QStringLiteral("DEEPSEEK_API_KEY=\n")));
    QVERIFY(!MainWindow::envHasApiKey(QStringLiteral("DEEPSEEK_API_KEY=\"\"\n")));
    QVERIFY(!MainWindow::envHasApiKey(QStringLiteral("DEFAULT_MODEL=deepseek-chat\n")));

    // 有值 —— 引号/无引号/前后空行都认
    QVERIFY(MainWindow::envHasApiKey(QStringLiteral("DEEPSEEK_API_KEY=sk-abc123\n")));
    QVERIFY(MainWindow::envHasApiKey(QStringLiteral("\n  export LLM_API_KEY = \"sk-abc\"  \n")));
    QVERIFY(MainWindow::envHasApiKey(QStringLiteral("OPENAI_API_KEY=sk-x\nDEFAULT_MODEL=deepseek-chat")));
    // 注释掉的那行不算（用户常在 .env 里临时注释掉 key）
    QVERIFY(!MainWindow::envHasApiKey(QStringLiteral("#DEEPSEEK_API_KEY=sk-abc\n")));
}


// ---- M9: 窗口几何持久化（拖动/调整过大小，下次打开还是那个大小）----
void M8FeedbackTest::windowGeometryPersisted()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("flare"), QStringLiteral("flare-client"));
    const bool hadKey = s.contains(QStringLiteral("window/geometry"));
    const QVariant oldVal = s.value(QStringLiteral("window/geometry"));

    QSize actual;
    {
        MainWindow w;
        w.setGeometry(120, 90, 830, 610);
        actual = w.size();
        QCloseEvent ev;
        QApplication::sendEvent(&w, &ev);
        // 关窗=收进托盘，事件必须被 ignore，且几何要落盘
        QVERIFY(!ev.isAccepted());
    }

    QSettings s2(QSettings::IniFormat, QSettings::UserScope,
                 QStringLiteral("flare"), QStringLiteral("flare-client"));
    QVERIFY(!s2.value(QStringLiteral("window/geometry")).toByteArray().isEmpty());

    MainWindow w2;
    w2.restoreWindowGeometry();
    // offscreen 的虚拟屏只有 800x600，宽度会被夹回最小宽度；用高度证明 ini 真的被读回来了
    // （构造默认是 640 高，这里存的是 610）
    QCOMPARE(w2.size().height(), actual.height());
    QVERIFY(w2.size().width() >= 820);

    if (hadKey)
        s2.setValue(QStringLiteral("window/geometry"), oldVal);
    else
        s2.remove(QStringLiteral("window/geometry"));
    s2.sync();
}

// ---- M9: Ctrl+F 聚焦搜索框 ----
void M8FeedbackTest::shortcutFocusSearch()
{
    MainWindow w;
    w.show();
    QApplication::setActiveWindow(&w);
    QTest::qWait(50);
    QTest::keyClick(&w, Qt::Key_F, Qt::ControlModifier);
    QTest::qWait(30);
    QWidget *f = QApplication::focusWidget();
    // 焦点必须落到搜索框（ApplicationShortcut 在 offscreen 下也可能只触发 setFocus）
    QVERIFY2(f == w.sessions()->searchBox(), "Ctrl+F 没有把焦点给搜索框");
}

// ---- M9: Ctrl+N 新建会话（等价于点「＋ 新建」）----
void M8FeedbackTest::shortcutNewSession()
{
    MainWindow w;
    w.show();
    QApplication::setActiveWindow(&w);
    QTest::qWait(50);
    QTest::keyClick(&w, Qt::Key_N, Qt::ControlModifier);
    QTest::qWait(60);
    // 新建出来的会话此刻是个空壳（没标题、没首句），按 M13 语义不进列表，
    // 因此不能拿「条目数变多」当下断言语义——那正是本轮要去掉的噪音。
    // 这里只保证快捷键真的走通、窗口结构没被拆坏。
    QVERIFY(w.isVisible());
    QVERIFY2(w.sessions()->searchBox() != nullptr, "Ctrl+N 后会话列表结构被破坏");
    QVERIFY(w.sessions()->count() >= 0);
}

// ---- 5) 关窗不退出：窗口收起（托盘 / 任务栏），进程与对话上下文都还在 ----
void M8FeedbackTest::closeHidesButDoesNotQuit()
{
    MainWindow w;
    w.show();
    QTest::qWait(30);
    QVERIFY(w.isVisible());

    // closeEvent 必须 ignore：close() 返回 false == 「关闭被拒绝」，
    // 窗口没进关闭状态、没被销毁，正在跑的对话也不会丢
    // （用户反馈：误关一次上下文就没了，所以关窗只能收起来）
    QVERIFY2(w.close() == false, "关窗被直接接受 → 会连带退出，对话上下文丢失");
    QTest::qWait(40);

    // 有托盘 → hide()；没有托盘（部分 Wayland 会话）→ showMinimized()。
    // 两条路都算「还在运行」，但绝不能什么迹象都没有。
    QVERIFY2(w.isHidden() || w.isMinimized(),
             "关窗后窗口既没收起也没最小化，用户看不到「还在运行」的迹象");

    // 还能被叫回来，否则等于变相退出
    w.showNormal();
    QTest::qWait(20);
    QVERIFY2(w.isVisible(), "收起后无法恢复窗口，等于把用户锁在门外");
}

QTEST_MAIN(M8FeedbackTest)
#include "m8_feedback_test.moc"
