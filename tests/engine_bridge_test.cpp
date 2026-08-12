#include <QtTest>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTimer>
#include "../engine-bridge/engine_bridge.h"

// ============================================================
// M1 引擎桥接测试
// 1) 纯函数测试：协议解析 / 请求构造（不依赖子进程）
// 2) 集成测试：真实 spawn `flare server`，实测协议交互
//    （集成测试在 PATH 有 flare 时运行；CI/无 flare 时跳过）
// ============================================================

class EngineBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    // ---- 纯函数 ----
    void parseValidJson();
    void parseInvalidJsonReturnsEmpty();
    void eventTypeExtraction();
    void buildRequestHasNewlineAndCompact();
    // ---- 集成 ----
    void versionHandshake();
    void pingPong();
    void listSessionsResponse();
};

void EngineBridgeTest::parseValidJson()
{
    const QJsonObject obj = parseFlareLine("{\"type\":\"pong\",\"ts\":123}");
    QCOMPARE(obj.value("type").toString(), QString("pong"));
    QCOMPARE(obj.value("ts").toInt(), 123);
}

void EngineBridgeTest::parseInvalidJsonReturnsEmpty()
{
    QVERIFY(parseFlareLine("not json").isEmpty());
    QVERIFY(parseFlareLine("").isEmpty());
    QVERIFY(parseFlareLine("[1,2,3]").isEmpty()); // 数组不是对象
}

void EngineBridgeTest::eventTypeExtraction()
{
    QJsonObject obj;
    obj.insert("type", "done");
    QCOMPARE(eventType(obj), QString("done"));
    QVERIFY(eventType(QJsonObject()).isEmpty());
}

void EngineBridgeTest::buildRequestHasNewlineAndCompact()
{
    QJsonObject req;
    req.insert("type", "ping");
    const QByteArray out = buildRequest(req);
    QVERIFY(out.endsWith('\n'));
    // compact：无多余空白
    QCOMPARE(out.trimmed(), QByteArray("{\"type\":\"ping\"}"));
}

// ---- 集成测试（需真实 flare server）----

static bool flareAvailable()
{
    // 检查 flare 可执行文件存在（PATH 或 ~/.flare/install）
    QProcess check;
    check.start(QStringLiteral("flare"), {QStringLiteral("--version")});
    if (!check.waitForStarted(2000))
        return false;
    check.waitForFinished(3000);
    return check.exitCode() == 0;
}

void EngineBridgeTest::versionHandshake()
{
    if (!flareAvailable())
        QSKIP("flare 不可用，跳过集成测试");

    EngineBridge bridge;
    QVERIFY(bridge.start());
    QSignalSpy spy(&bridge, &EngineBridge::eventReceived);

    bridge.queryVersion();
    QVERIFY(spy.wait(5000));

    bool sawVersion = false;
    for (const auto &args : spy) {
        const QJsonObject obj = args.at(0).toJsonObject();
        if (obj.value("type").toString() == "version") {
            sawVersion = true;
            QVERIFY(!obj.value("protocol").toString().isEmpty());
            QVERIFY(!obj.value("engine").toString().isEmpty());
        }
    }
    QVERIFY2(sawVersion, "应收到 version 响应");
    QVERIFY(!bridge.protocolVersion().isEmpty());
    bridge.stop();
}

void EngineBridgeTest::pingPong()
{
    if (!flareAvailable())
        QSKIP("flare 不可用，跳过集成测试");

    EngineBridge bridge;
    QVERIFY(bridge.start());
    QSignalSpy spy(&bridge, &EngineBridge::eventReceived);

    bridge.ping();
    QVERIFY(spy.wait(5000));

    bool sawPong = false;
    for (const auto &args : spy) {
        const QJsonObject obj = args.at(0).toJsonObject();
        if (obj.value("type").toString() == "pong")
            sawPong = true;
    }
    QVERIFY2(sawPong, "应收到 pong 响应");
    bridge.stop();
}

void EngineBridgeTest::listSessionsResponse()
{
    if (!flareAvailable())
        QSKIP("flare 不可用，跳过集成测试");

    EngineBridge bridge;
    QVERIFY(bridge.start());
    QSignalSpy spy(&bridge, &EngineBridge::eventReceived);

    bridge.listSessions();
    QVERIFY(spy.wait(5000));

    bool sawSessions = false;
    for (const auto &args : spy) {
        const QJsonObject obj = args.at(0).toJsonObject();
        if (obj.value("type").toString() == "sessions")
            sawSessions = true;
    }
    QVERIFY2(sawSessions, "应收到 sessions 响应");
    bridge.stop();
}

QTEST_MAIN(EngineBridgeTest)
#include "engine_bridge_test.moc"
