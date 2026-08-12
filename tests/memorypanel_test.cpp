#include <QtTest>
#include <QApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidget>
#include "../src/memorypanel.h"
#include "../src/sessionlistwidget.h"
#include "../src/mainwindow.h"
#include "engine_bridge.h"

// ============================================================
// M3-2 记忆查看面板测试
// 1) UI 结构：列表 + 刷新按钮存在
// 2) memories 事件 → 列表填充（content/type 徽标/时间）
// 3) 空内容记忆被过滤
// 4) 集成：MainWindow 侧边栏含「记忆」入口，get_memories 真实往返
// ============================================================

class MemoryPanelTest : public QObject
{
    Q_OBJECT

private slots:
    void widgetHasListAndRefresh();
    void memoriesEventPopulatesList();
    void emptyContentFiltered();
    void mainWindowHasMemoryEntry();
    void getMemoriesRoundTrip();
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

void MemoryPanelTest::widgetHasListAndRefresh()
{
    EngineBridge bridge;
    MemoryPanel p(&bridge);
    QVERIFY(p.list() != nullptr);
    QVERIFY(p.refreshButton() != nullptr);
    QVERIFY(p.refreshButton()->text().contains(QStringLiteral("刷新")));
}

void MemoryPanelTest::memoriesEventPopulatesList()
{
    EngineBridge bridge;
    MemoryPanel p(&bridge);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", 1}, {"content", "用户喜欢紫色主题"}, {"type", "preference"}, {"created_at", "2026-08-12 10:00:00"}});
    arr.append(QJsonObject{{"id", 2}, {"content", "测试笔记"}, {"type", "note"}, {"created_at", "2026-08-11 09:00:00"}});

    p.onEngineEvent(QJsonObject{{"type", "memories"}, {"memories", arr}});
    QCOMPARE(p.count(), 2);
    // preference 带类型徽标前缀，note 默认不显示
    QVERIFY(p.list()->item(0)->text().contains("[preference]"));
    QVERIFY(p.list()->item(0)->text().contains("用户喜欢紫色主题"));
    QVERIFY(p.list()->item(0)->text().contains("2026-08-12"));
    QVERIFY(!p.list()->item(1)->text().contains("[note]"));
}

void MemoryPanelTest::emptyContentFiltered()
{
    EngineBridge bridge;
    MemoryPanel p(&bridge);

    QJsonArray arr;
    arr.append(QJsonObject{{"id", 1}, {"content", ""}, {"type", "note"}});
    arr.append(QJsonObject{{"id", 2}, {"content", "   "}, {"type", "note"}});
    p.onEngineEvent(QJsonObject{{"type", "memories"}, {"memories", arr}});
    QCOMPARE(p.count(), 0);
}

void MemoryPanelTest::mainWindowHasMemoryEntry()
{
    EngineBridge bridge;
    MainWindow w;
    QVERIFY(w.sessions() != nullptr);
    QVERIFY(w.sessions()->memoryButton() != nullptr);
    QVERIFY(w.sessions()->memoryButton()->text().contains(QStringLiteral("记忆")));
}

void MemoryPanelTest::getMemoriesRoundTrip()
{
    if (!flareAvailable())
        QSKIP("flare 不可用，跳过集成测试");

    EngineBridge bridge;
    QVERIFY(bridge.start());

    QSignalSpy spy(&bridge, &EngineBridge::eventReceived);
    bridge.getMemories();
    QVERIFY2(spy.wait(5000), "应收到 memories 响应");

    bool sawMemories = false;
    for (const auto &args : spy) {
        const QJsonObject obj = args.at(0).toJsonObject();
        if (obj.value("type").toString() == "memories") {
            sawMemories = true;
            QVERIFY(obj.value("memories").isArray());
        }
    }
    QVERIFY2(sawMemories, "应收到 memories 事件（可为空数组）");
    bridge.stop();
}

QTEST_MAIN(MemoryPanelTest)
#include "memorypanel_test.moc"
