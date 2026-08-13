#include <QtTest>
#include <QApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidget>
#include <QDateTime>
#include "../src/sessionlistwidget.h"
#include "engine_bridge.h"

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
    void widgetHasListAndButtons();
    void sessionsEventPopulatesList();
    void clickEmitsSessionSelected();
    void createThenDeleteRoundTrip();
    // M7-2
    void searchBoxExistsAndFilters();
    void searchEventPopulatesList();
    void archivedEventPopulatesList();
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
    QCOMPARE(w.list()->item(0)->text(), QString("会话甲"));
    QCOMPARE(w.list()->item(0)->data(Qt::UserRole).toString(), QString("sess-a"));
    QCOMPARE(w.list()->item(1)->text(), QString("会话乙"));

    // 空标题兜底为「新会话」
    QJsonArray arr2;
    QJsonObject s3{{"id", "sess-c"}};
    arr2.append(s3);
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr2}});
    QCOMPARE(w.count(), 1);
    QCOMPARE(w.list()->item(0)->text(), QString("新会话"));
}

void SessionListWidgetTest::clickEmitsSessionSelected()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "sess-x"}, {"title", "会话X"}});
    w.onEngineEvent(QJsonObject{{"type", "sessions"}, {"sessions", arr}});

    QSignalSpy spy(&w, &SessionListWidget::sessionSelected);
    w.list()->setCurrentRow(0);
    w.list()->itemClicked(w.list()->item(0)); // 模拟点击
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
    QCOMPARE(w.list()->item(0)->text(), QString("灵感记录"));
    QCOMPARE(w.list()->item(0)->data(Qt::UserRole).toString(), QString("hit-1"));

    // 搜索框被清空（关键词失效）时，旧 search 响应不覆盖列表
    QJsonArray arr2;
    arr2.append(QJsonObject{{"id", "stale"}, {"title", "过期"}});
    w.searchBox()->clear();
    w.onEngineEvent(QJsonObject{{"type", "search_sessions"}, {"sessions", arr2}});
    QCOMPARE(w.count(), 1); // 仍是 hit-1，未被 stale 覆盖
    QCOMPARE(w.list()->item(0)->data(Qt::UserRole).toString(), QString("hit-1"));
}

// M7-2: archived_sessions 事件 → 列表填充
void SessionListWidgetTest::archivedEventPopulatesList()
{
    EngineBridge bridge;
    SessionListWidget w(&bridge);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", "arch-1"}, {"title", "归档会话A"}, {"updatedAt", "2026-08-01T10:00:00"}});
    w.onEngineEvent(QJsonObject{{"type", "archived_sessions"}, {"sessions", arr}});
    QCOMPARE(w.count(), 1);
    QCOMPARE(w.list()->item(0)->text().startsWith(QString("归档会话A")), true);
    QCOMPARE(w.list()->item(0)->data(Qt::UserRole).toString(), QString("arch-1"));
}

QTEST_MAIN(SessionListWidgetTest)
#include "sessionlistwidget_test.moc"
