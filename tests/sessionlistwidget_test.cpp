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

QTEST_MAIN(SessionListWidgetTest)
#include "sessionlistwidget_test.moc"
