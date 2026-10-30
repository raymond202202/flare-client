#include <QtTest>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTimer>
#include <QTemporaryDir>
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
    void engineCandidatesCoverSystemPaths();
    void engineCandidatesHonorExplicitOverride();
    void pickEngineSkipsMissingAndNonExecutable();
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

// 生产安装版曾因「只查与本程序同目录」而一启动就报引擎启动失败：
// RPM 把 GUI 装到 /usr/bin、引擎装到 /usr/lib64/flare-client/，同目录永远是空的。
void EngineBridgeTest::engineCandidatesCoverSystemPaths()
{
    const QStringList c = engineCandidates();
    QVERIFY2(!c.isEmpty(), "候选列表不能为空");
#ifdef Q_OS_LINUX
    QVERIFY2(c.contains(QStringLiteral("/usr/lib64/flare-client/flare")),
             "必须查 RPM 安装位置 /usr/lib64/flare-client/flare");
    QVERIFY2(c.contains(QStringLiteral("/usr/lib/flare-client/flare")),
             "必须查 deb 安装位置 /usr/lib/flare-client/flare");
#endif
}

void EngineBridgeTest::engineCandidatesHonorExplicitOverride()
{
    const QByteArray old = qgetenv("FLARE_ENGINE_BIN");
    qputenv("FLARE_ENGINE_BIN", "/tmp/my-flare");
    const QStringList c = engineCandidates();
    QCOMPARE(c.value(0), QStringLiteral("/tmp/my-flare"));   // 显式配置必须排第一
    if (old.isEmpty())
        qunsetenv("FLARE_ENGINE_BIN");
    else
        qputenv("FLARE_ENGINE_BIN", old);
}

void EngineBridgeTest::pickEngineSkipsMissingAndNonExecutable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString plain = dir.filePath(QStringLiteral("plain"));
    QFile f(plain);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not executable");
    f.close();
    QFile::setPermissions(plain, QFile::ReadOwner | QFile::WriteOwner);

    const QString exe = dir.filePath(QStringLiteral("exe"));
    QFile g(exe);
    QVERIFY(g.open(QIODevice::WriteOnly));
    g.write("#!/bin/sh\n");
    g.close();
    QFile::setPermissions(exe, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    // 不存在的跳过、存在的但没执行位也跳过、真可执行的才选
    const QStringList candidates{
        dir.filePath(QStringLiteral("missing")), plain, QString(), exe};
    QCOMPARE(pickEngine(candidates), exe);
    // 一个都没有 → 空串（调用方退回 PATH）
    QVERIFY(pickEngine({dir.filePath(QStringLiteral("missing"))}).isEmpty());
    QVERIFY(pickEngine({}).isEmpty());
}

QTEST_MAIN(EngineBridgeTest)
#include "engine_bridge_test.moc"
