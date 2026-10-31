#include <QtTest>
#include <QApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidget>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include "../src/sessionlistwidget.h"
#include "engine_bridge.h"

// 列表里所有分组标题（不可选中、无 sessionId 的项）
static QStringList sectionHeaders(QListWidget *list)
{
    QStringList out;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *it = list->item(i);
        if (it->data(Qt::UserRole).toString().isEmpty() && !(it->flags() & Qt::ItemIsSelectable))
            out << it->text().trimmed();
    }
    return out;
}

// ============================================================
// M3-1 会话列表侧边栏测试
// 1) UI 结构：列表 + 新建/删除按钮存在
// 2) sessions 事件 → 列表填充（id 存 UserRole、标题正确）
// 3) 点选列表项 → sessionSelected 信号
// 4) 新建按钮 → 触发 create_session（真实引擎集成）
// 5) 删除按钮 → 触发 delete_session（真实引擎集成）
// ============================================================

class SessionListWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void widgetHasListAndButtons();
    void sessionsEventPopulatesList();
    void clickEmitsSessionSelected();
    void createThenDeleteRoundTrip();
    // M7-2
    void searchBoxExistsAndFilters();
    void searchEventPopulatesList();
    // 0.6.184：搜索回执校对 + 输入防抖
    void staleSearchResponseIsDropped();
    void searchKeystrokesAreDebounced();
    // 0.6.185：搜索范围按视图走（回收站可搜）+ 跨视图迟到回执丢弃
    void searchInArchiveViewAsksArchivedScope();
    void crossViewSearchResponseIsDropped();
    void archivedEventPopulatesList();
    // M13
    void sessionsGroupedByDay();
    void searchViewHasNoSectionHeaders();
    void shortStampKeepsYearAcrossYears();
    void keyboardNavSwitchesSessionAndSkipsHeaders();
    void searchBoxArrowDownJumpsToList();
    // M14-2
    void pinnedSessionSticksToTopAndSurvivesRestart();
};

static bool flareAvailable()
{
    QProcess check;
    check.start(QStringLiteral("flare"), {QStringLiteral("--version")});
    if (!check.waitForStarted(2000))
        return false;
    check.waitForFinished(3000);
    return check.exitCode() == 0;
}

void SessionListWidgetTest::widgetHasListAndButtons()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    QVERIFY(w.list() != nullptr);
    QVERIFY(w.newButton() != nullptr);
    QVERIFY(w.deleteButton() != nullptr);
    QVERIFY(w.newButton()->text().contains(QStringLiteral("新建")));
}

void SessionListWidgetTest::sessionsEventPopulatesList()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    QJsonArray arr;
    QJsonObject s1{{"id", "sess-a"}, {"title", "会话甲"}, {"messageCount", 3}};
    QJsonObject s2{{"id", "sess-b"}, {"title", "会话乙"}, {"messageCount", 0}};
    arr.append(s1);
    arr.append(s2);

    QJsonObject ev{{"type", "sessions"}, {"sessions", arr}};
    w.onEngineEvent(ev);

    QCOMPARE(w.count(), 2);
    QCOMPARE(w.sessionItem(0)->text(), QString("会话甲"));
    QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("sess-a"));
    QCOMPARE(w.sessionItem(1)->text(), QString("会话乙"));

    // M13: 没说过的空会话（占位标题 + 无预览 + 无别名）不进列表 —— 一排
    // 「新会话」用户也点不出内容，纯噪音。
    QJsonArray arr2;
    QJsonObject s3{{"id", "sess-c"}};
    arr2.append(s3);
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr2}});
    QCOMPARE(w.count(), 0);

    // 只要说过一句话（引擎给了 preview），就照常显示
    QJsonArray arr3;
    arr3.append(QJsonObject{{"id", "sess-d"}, {"preview", "帮我看下这个报错"}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr3}});
    QCOMPARE(w.count(), 1);
    QCOMPARE(w.sessionItem(0)->text(), QString("帮我看下这个报错"));
}

void SessionListWidgetTest::clickEmitsSessionSelected()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "sess-x"}, {"title", "会话X"}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});

    QSignalSpy spy(&w, &SessionListWidget::sessionSelected);
    w.list()->setCurrentItem(w.sessionItem(0));
    w.list()->itemClicked(w.sessionItem(0)); // 模拟点击
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("sess-x"));
}

void SessionListWidgetTest::createThenDeleteRoundTrip()
{
    if (!flareAvailable())
        QSKIP("flare 不可用，跳过集成测试");

    EngineBridge bridge;
    QVERIFY(bridge.start());
    SessionListWidget w(&bridge);

    // 新会话（唯一 id）
    const QString sid = QStringLiteral("t-m31-") + QString::number(QDateTime::currentMSecsSinceEpoch());
    bridge.createSession(sid, QStringLiteral("测试会话"));

    QSignalSpy spy(&bridge, &EngineBridge::eventReceived);
    // 等待 create ok
    QVERIFY2(spy.wait(5000), "应收到 create_session 的 ok 回执");

    // 轮询等待 sessions 事件出现 sid
    bool found = false;
    QSignalSpy sessSpy(&bridge, &EngineBridge::eventReceived);
    bridge.listSessions();
    for (int i = 0; i < 20; ++i) {
        if (sessSpy.wait(500)) {
            for (const auto &args : sessSpy) {
                const QJsonObject obj = args.at(0).toJsonObject();
                if (obj.value("type").toString() != "sessions")
                    continue;
                const QJsonArray sessions = obj.value("sessions").toArray();
                for (const auto &v : sessions) {
                    if (v.toObject().value("id").toString() == sid)
                        found = true;
                }
            }
            sessSpy.clear();
        }
        if (found)
            break;
    }
    QVERIFY2(found, "list_sessions 应包含新建会话");

    // 删除会话
    bridge.deleteSession(sid);
    QSignalSpy delSpy(&bridge, &EngineBridge::eventReceived);
    QVERIFY2(delSpy.wait(5000), "应收到 delete_session 的 ok 回执");

    bridge.stop();
}

// M7-2: 搜索框存在 + 输入关键词时触发 search_sessions 请求
void SessionListWidgetTest::searchBoxExistsAndFilters()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    QVERIFY(w.searchBox() != nullptr);
    QVERIFY(w.archiveButton() != nullptr);

    QSignalSpy spy(&bridge, &EngineBridge::requestSent);
    w.searchBox()->setText(QStringLiteral("灵感"));
    // 0.6.184：搜索改为 250ms 防抖，输入后要等防抖到期才会真发请求
    QCOMPARE(spy.count(), 0);
    QTest::qWait(400);
    QCOMPARE(spy.count(), 1);
    const QJsonObject req = spy.at(0).at(0).toJsonObject();
    QCOMPARE(req.value(QStringLiteral("type")).toString(), QStringLiteral("search_sessions"));
    QCOMPARE(req.value(QStringLiteral("query")).toString(), QStringLiteral("灵感"));

    // 清空 → 回到 recent_sessions
    spy.clear();
    w.searchBox()->clear();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toJsonObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("recent_sessions"));
}

// M7-2: search_sessions 事件 → 列表填充
void SessionListWidgetTest::searchEventPopulatesList()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    w.searchBox()->setText(QStringLiteral("灵感"));

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "hit-1"}, {"title", "灵感记录"}});
    w.onEngineEvent(QJsonObject{{"type", "search_sessions"}, {"sessions", arr}});
    QCOMPARE(w.count(), 1);
    QCOMPARE(w.sessionItem(0)->text(), QString("灵感记录"));
    QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("hit-1"));

    // 搜索框被清空（关键词失效）时，旧 search 响应不覆盖列表
    QJsonArray arr2;
    arr2.append(QJsonObject{{"id", "stale"}, {"title", "过期"}});
    w.searchBox()->clear();
    w.onEngineEvent(QJsonObject{{"type", "search_sessions"}, {"sessions", arr2}});
    QCOMPARE(w.count(), 1); // 仍是 hit-1，未被 stale 覆盖
    QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("hit-1"));
}

// 0.6.184：引擎回执里带 query 回显，客户端拿它校对搜索框 —— 迟到的旧关键词
// 回执必须丢弃，否则列表会被顶成上一个关键词的结果，而搜索框写着新词。
void SessionListWidgetTest::staleSearchResponseIsDropped()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    w.searchBox()->setText(QStringLiteral("灵感记录"));

    QJsonArray stale;
    stale.append(QJsonObject{{"id", "old-1"}, {"title", "旧关键词命中"}});
    w.onEngineEvent(QJsonObject{{"type", "search_sessions"},
                               {"query", "灵感"},
                               {"sessions", stale}});
    QCOMPARE(w.count(), 0);   // 迟到响应被丢

    QJsonArray fresh;
    fresh.append(QJsonObject{{"id", "new-1"}, {"title", "新关键词命中"}});
    w.onEngineEvent(QJsonObject{{"type", "search_sessions"},
                               {"query", "灵感记录"},
                               {"sessions", fresh}});
    QCOMPARE(w.count(), 1);
    QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("new-1"));
}

// 0.6.184：逐字敲不逐字查 —— 250ms 内连打三个字只发一次全表查询
void SessionListWidgetTest::searchKeystrokesAreDebounced()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    w.searchBox()->setText(QStringLiteral("灵"));
    w.searchBox()->setText(QStringLiteral("灵感"));
    w.searchBox()->setText(QStringLiteral("灵感记"));
    QCOMPARE(w.searchRequestsSent(), 0);   // 防抖期内一次都没发
    QTest::qWait(400);
    QCOMPARE(w.searchRequestsSent(), 1);   // 三次按键合并成一次查询

    // 清空搜索立即回到最近会话视图，不等防抖、也不多发查询
    w.searchBox()->clear();
    QCOMPARE(w.searchRequestsSent(), 1);
}

// M13: 会话按「今天 / 昨天 / 最近 7 天 / 更早」分组，置顶自成一组
void SessionListWidgetTest::sessionsGroupedByDay()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    const QDateTime now = QDateTime::currentDateTime();
    // M15-3：引擎下发的时间一律是 RFC3339 UTC（带 Z）。这里按同一口径造数据：
    // 本地时刻 → 转 UTC 再序列化，解析回来还是同一个瞬间。
    // 以前这里写的是 toString(ISODateWithMs) 的裸串，没有时区标记；
    // 现在裸串按 UTC 解释，会让「今天」的会话被推到明天、分组整个错位。
    const auto iso = [](const QDateTime &dt) {
        return dt.toUTC().toString(Qt::ISODate);
    };

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "s-today"}, {"title", "今天聊的"},
                           {"updatedAt", iso(now)}});
    arr.append(QJsonObject{{"id", "s-yest"}, {"title", "昨天聊的"},
                           {"updatedAt", iso(now.addDays(-1))}});
    arr.append(QJsonObject{{"id", "s-old"}, {"title", "很久以前"},
                           {"updatedAt", iso(now.addDays(-40))}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});

    // 三个会话三个分组，外加标题行不算会话
    QCOMPARE(w.count(), 3);
    QCOMPARE(sectionHeaders(w.list()),
             QStringList({"今天", "昨天", "更早"}));
    QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("s-today"));
    QCOMPARE(w.sessionItem(2)->data(Qt::UserRole).toString(), QString("s-old"));

    // 分组标题行不能当会话选中/触发信号
    QSignalSpy spy(&w, &SessionListWidget::sessionSelected);
    QListWidgetItem *header = w.list()->item(0);
    QCOMPARE(header->data(Qt::UserRole).toString(), QString());
    w.list()->itemClicked(header);
    QCOMPARE(spy.count(), 0);
}

// M7-2 / M8: archived_sessions 事件 → 归档视图内填充列表
// M8 起语义收紧：只在归档视图（回收站）内填充，否则会把「最近会话」列表顶掉
// M13: 搜索结果是平铺的，不该插「更早」这种分组标题
void SessionListWidgetTest::searchViewHasNoSectionHeaders()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    const QDateTime now = QDateTime::currentDateTime();
    QJsonArray arr;
    arr.append(QJsonObject{{"id", "s1"}, {"title", "主题色讨论"},
                           {"updatedAt", now.toUTC().toString(Qt::ISODate)}});
    arr.append(QJsonObject{{"id", "s2"}, {"title", "主题色笔记"},
                           {"updatedAt", now.addDays(-40).toUTC().toString(Qt::ISODate)}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});
    QVERIFY(!sectionHeaders(w.list()).isEmpty());

    auto *box = w.findChild<QLineEdit *>(QStringLiteral("sessionSearch"));
    QVERIFY(box);
    box->setText(QStringLiteral("主题色"));
    // 搜索态下重渲染：不再有分组标题，全部平铺
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});
    QVERIFY2(sectionHeaders(w.list()).isEmpty(), "搜索视图不该有分组标题");
}

void SessionListWidgetTest::archivedEventPopulatesList()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    w.showArchived(); // M8: 先进入回收站/归档箱视图

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "arch-1"}, {"title", "归档会话A"}, {"updatedAt", "2026-08-01T10:00:00"}});
    w.onEngineEvent(QJsonObject{{"type", "archived_sessions"}, {"sessions", arr}});
    QCOMPARE(w.count(), 1);
    QCOMPARE(w.sessionItem(0)->text().startsWith(QString("归档会话A")), true);
    QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("arch-1"));
}

// M13: 条目副行的时间戳。今天只给时刻（标题已说是哪天），今年给月日，
// 跨年必须带年份 —— 否则「03-15」到底是今年还是去年，用户只能点进去猜。
void SessionListWidgetTest::shortStampKeepsYearAcrossYears()
{
    // M15-3：这个测试原来用「无时区标记」的串（"2026-10-30T09:05:00.000"）当本地时间，
    // 断言出 "09:05"。现在裸串一律按 UTC 解释，那么写法必须改成与时区无关的：
    // 用本地时刻构造 → 转 UTC 带 Z 下发 → 解析回来还是原来那个本地时刻。
    // 这样 CI 跑在 UTC、用户跑在 +08:00，结论一致。
    const QDateTime now = QDateTime(QDate(2026, 10, 30), QTime(14, 20));
    const auto utcIso = [](const QDateTime &dt) { return dt.toUTC().toString(Qt::ISODate); };

    const QDateTime sameDay = QDateTime(QDate(2026, 10, 30), QTime(9, 5));
    QCOMPARE(SessionListWidget::shortStamp(utcIso(sameDay), now),
             sameDay.toString(QStringLiteral("HH:mm")));

    const QDateTime thisYear = QDateTime(QDate(2026, 8, 13), QTime(9, 5));
    QCOMPARE(SessionListWidget::shortStamp(utcIso(thisYear), now), QStringLiteral("08-13"));

    const QDateTime lastYear = QDateTime(QDate(2025, 3, 15), QTime(9, 5));
    QCOMPARE(SessionListWidget::shortStamp(utcIso(lastYear), now), QStringLiteral("2025-03-15"));

    // M15-3 回归：裸串（无时区标记）必须与同一时刻的 Z 串得出一样的结果，
    // 且都不是「按本地时间解释」——以前正是这一步差了 8 小时。
    const QString bare = QStringLiteral("2026-10-30T09:05:00.000");
    QCOMPARE(SessionListWidget::shortStamp(bare, now),
             SessionListWidget::shortStamp(bare + QStringLiteral("Z"), now));

    // 非法时间不崩，原样返回
    QCOMPARE(SessionListWidget::shortStamp(QStringLiteral("2026-13-99"), now),
             QStringLiteral("2026-13-99"));
}


// M14: 键盘 ↑/↓ 必须真的切会话，而且不能停在「今天」「昨天」这种分组标题上
// （标题行是 NoItemFlags，QListWidget 的键盘导航本来会跳过它，这里把它钉成契约）
void SessionListWidgetTest::keyboardNavSwitchesSessionAndSkipsHeaders()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    const QDateTime now = QDateTime::currentDateTime();
    const auto iso = [](const QDateTime &dt) { return dt.toUTC().toString(Qt::ISODate); };
    QJsonArray arr;
    arr.append(QJsonObject{{"id", "s-today"}, {"title", "今天聊的"}, {"updatedAt", iso(now)}});
    arr.append(QJsonObject{{"id", "s-yest"}, {"title", "昨天聊的"}, {"updatedAt", iso(now.addDays(-1))}});
    arr.append(QJsonObject{{"id", "s-old"}, {"title", "很久以前"}, {"updatedAt", iso(now.addDays(-40))}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});

    // 三个分组标题把三个会话隔开，正好试「跨组下移」
    QCOMPARE(w.count(), 3);
    w.list()->setCurrentItem(w.sessionItem(0));
    QCOMPARE(w.currentSessionId(), QString("s-today"));

    QSignalSpy spy(&w, &SessionListWidget::sessionSelected);
    w.list()->setFocus();
    QTest::keyClick(w.list(), Qt::Key_Down);

    QVERIFY2(spy.count() >= 1, "按 ↓ 之后没有任何 sessionSelected —— 键盘只动了高亮没切会话");
    QCOMPARE(spy.last().at(0).toString(), QString("s-yest"));
    // 落到分组标题上就等于「选中了个寂寞」，必须直接是下一个真会话
    QCOMPARE(w.currentSessionId(), QString("s-yest"));

    // 同一项上再按一次，不该把已打开的会话重新加载一遍
    const int before = spy.count();
    QTest::keyClick(w.list(), Qt::Key_Down);
    QCOMPARE(spy.count(), before + 1);
}

// M14: 在搜索框里敲完关键字，按 ↓ 应该能直接进结果，不用再摸鼠标
void SessionListWidgetTest::searchBoxArrowDownJumpsToList()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    const QDateTime now = QDateTime::currentDateTime();
    QJsonArray arr;
    arr.append(QJsonObject{{"id", "r-1"}, {"title", "报错怎么读"},
                           {"updatedAt", now.toUTC().toString(Qt::ISODate)}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});
    QCOMPARE(w.count(), 1);

    QSignalSpy spy(&w, &SessionListWidget::sessionSelected);
    w.searchBox()->setFocus();
    QTest::keyClick(w.searchBox(), Qt::Key_Down);

    QVERIFY2(w.list()->currentItem() != nullptr, "在搜索框按 ↓ 没有把选中落到列表上");
    QCOMPARE(w.currentSessionId(), QString("r-1"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("r-1"));
}

void SessionListWidgetTest::initTestCase()
{
    // 置顶记录写 QSettings。测试必须关进 Qt 的测试沙箱（~/.qttest/config），
    // 否则跑一次单测就把用户真实 ~/.config/flare/flare-client.ini 的置顶清空。
    QStandardPaths::setTestModeEnabled(true);
    QSettings(QSettings::IniFormat, QSettings::UserScope,
              QStringLiteral("flare"), QStringLiteral("flare-client")).clear();
}

// M14-2: 置顶是会话一多以后唯一的抓手。排序全靠 applySessions 里「置顶项单独先插」
// 这一条，谁把插入顺序改掉，置顶就悄悄失效而且没人发现 —— 把它钉成契约。
void SessionListWidgetTest::pinnedSessionSticksToTopAndSurvivesRestart()
{
    const QDateTime now = QDateTime::currentDateTime();
    const auto iso = [](const QDateTime &dt) { return dt.toUTC().toString(Qt::ISODate); };
    QJsonArray arr;
    arr.append(QJsonObject{{"id", "p-new"}, {"title", "今天聊的"}, {"updatedAt", iso(now)}});
    arr.append(QJsonObject{{"id", "p-mid"}, {"title", "三天前聊的"}, {"updatedAt", iso(now.addDays(-3))}});
    arr.append(QJsonObject{{"id", "p-old"}, {"title", "上个月聊的"}, {"updatedAt", iso(now.addDays(-40))}});
    const auto feed = [&arr](SessionListWidget &w) {
        w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});
    };

    {
        EngineBridge bridge;
        SessionListWidget w(&bridge);
        feed(w);
        QCOMPARE(w.count(), 3);
        QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("p-new"));

        // 置顶最老的那条
        w.list()->setCurrentItem(w.sessionItem(2));
        w.pinCurrent();
        feed(w); // refresh 是异步的，这里手动重放一次引擎回包

        QCOMPARE(w.count(), 3); // 置顶不能把这条复制成两份
        QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QString("p-old"));
        QVERIFY2(w.sessionItem(0)->text().startsWith(QString("\U0001F4CC")),
                 "置顶项没有 📌 前缀，用户看不出它凭什么排在最上面");
        // 置顶自成一组，标题行必须排在这条前面
        QVERIFY(w.list()->item(0)->data(Qt::UserRole).toString().isEmpty());
        QVERIFY(w.list()->item(0)->text().contains(QStringLiteral("置顶")));
    }

    // 重开一个 widget = 模拟重启客户端，置顶必须还在（说明真落盘了，不是内存态）
    {
        EngineBridge bridge2;
        SessionListWidget w2(&bridge2);
        feed(w2);
        QCOMPARE(w2.sessionItem(0)->data(Qt::UserRole).toString(), QString("p-old"));

        // 取消置顶 → 回到时间序
        w2.list()->setCurrentItem(w2.sessionItem(0));
        w2.pinCurrent();
        feed(w2);
        QCOMPARE(w2.sessionItem(0)->data(Qt::UserRole).toString(), QString("p-new"));
        const QStringList heads = sectionHeaders(w2.list());
        for (const QString &h : heads)
            QVERIFY2(!h.contains(QStringLiteral("置顶")), "取消置顶后还留着「置顶」标题行");
    }
}


// 0.6.185：回收站里也要能搜。误删会话之后用户正是靠关键词把它捞回来恢复的，
// 而引擎按集合区分搜索范围 —— 宿主必须把「我现在站在回收站」告诉引擎，
// 否则在回收站里搜什么都只能得到空列表。
void SessionListWidgetTest::searchInArchiveViewAsksArchivedScope()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    w.showArchived();   // 切到回收站视图

    QSignalSpy spy(&bridge, &EngineBridge::requestSent);
    w.searchBox()->setText(QStringLiteral("灵感"));
    QTest::qWait(400);
    QVERIFY(spy.count() >= 1);
    const QJsonObject req = spy.at(spy.count() - 1).at(0).toJsonObject();
    QCOMPARE(req.value(QStringLiteral("type")).toString(), QStringLiteral("search_sessions"));
    QCOMPARE(req.value(QStringLiteral("query")).toString(), QStringLiteral("灵感"));
    QCOMPARE(req.value(QStringLiteral("archived")).toBool(false), true);

    // 回到主列表：同一个关键词，范围必须回到默认（不带 archived）
    spy.clear();
    w.showRecent();
    w.searchBox()->setText(QStringLiteral("灵感"));
    QTest::qWait(400);
    QVERIFY(spy.count() >= 1);
    const QJsonObject req2 = spy.at(spy.count() - 1).at(0).toJsonObject();
    QCOMPARE(req2.value(QStringLiteral("type")).toString(), QStringLiteral("search_sessions"));
    QCOMPARE(req2.value(QStringLiteral("archived")).toBool(false), false);
}

// 0.6.185：关键词相同但视图不同，回执同样是过期的 —— 光比 query 挡不住。
// 场景：在回收站里搜过「灵感」→ 切回主列表再搜「灵感」→ 归档那份回执后到。
void SessionListWidgetTest::crossViewSearchResponseIsDropped()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);
    w.searchBox()->setText(QStringLiteral("灵感"));

    // 归档范围的回执（当前在主列表）→ 丢弃
    QJsonArray fromArchive;
    fromArchive.append(QJsonObject{{"id", "arch-9"}, {"title", "回收站里的命中"}});
    w.onEngineEvent(QJsonObject{{"type", "search_sessions"},
                                {"query", "灵感"},
                                {"archived", true},
                                {"sessions", fromArchive}});
    QCOMPARE(w.count(), 0);

    // 同关键词、当前视图范围的回执 → 正常上屏
    QJsonArray fresh;
    fresh.append(QJsonObject{{"id", "live-9"}, {"title", "主列表命中"}});
    w.onEngineEvent(QJsonObject{{"type", "search_sessions"},
                                {"query", "灵感"},
                                {"sessions", fresh}});
    QCOMPARE(w.count(), 1);
    QCOMPARE(w.sessionItem(0)->data(Qt::UserRole).toString(), QStringLiteral("live-9"));
}

QTEST_MAIN(SessionListWidgetTest)
#include "sessionlistwidget_test.moc"
