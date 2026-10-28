#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include "versionpanel.h"
#include "updatechecker.h"

// ============================================================
// 版本与更新 —— 单元测试
//
// 覆盖三件事：
//   1. 版本号比较（本地 vs 远端 Release 的判定基准，纯函数，不联网）
//   2. 面板同时展示 GUI 版本 + 内核名字/版本（用户新要求）
//   3. 两条「检查更新」互相独立（内核通道状态变化不影响客户端通道）
// ============================================================
class VersionTest : public QObject
{
    Q_OBJECT

private slots:
    // ---------- 1. 版本号规范化与比较 ----------
    void normalize()
    {
        QCOMPARE(normalizeVersion(QStringLiteral("v0.6.161")), QStringLiteral("0.6.161"));
        QCOMPARE(normalizeVersion(QStringLiteral("V0.6.161")), QStringLiteral("0.6.161"));
        QCOMPARE(normalizeVersion(QStringLiteral("  0.5.0  ")), QStringLiteral("0.5.0"));
        QCOMPARE(normalizeVersion(QString()), QString());
    }

    void compare()
    {
        QVERIFY(compareVersions(QStringLiteral("0.5.0"), QStringLiteral("0.4.0")) > 0);
        QVERIFY(compareVersions(QStringLiteral("0.4.0"), QStringLiteral("0.5.0")) < 0);
        QCOMPARE(compareVersions(QStringLiteral("0.4.0"), QStringLiteral("v0.4.0")), 0);
        // 段数不同按缺省 0 补齐
        QCOMPARE(compareVersions(QStringLiteral("0.6"), QStringLiteral("0.6.0")), 0);
        QVERIFY(compareVersions(QStringLiteral("0.6.161"), QStringLiteral("0.6.99")) > 0);
        QVERIFY(compareVersions(QStringLiteral("0.10.0"), QStringLiteral("0.9.9")) > 0);
        // 预发布后缀不参与比较
        QCOMPARE(compareVersions(QStringLiteral("0.5.0-beta.1"), QStringLiteral("0.5.0")), 0);
        QVERIFY(compareVersions(QStringLiteral("1.0.0"), QStringLiteral("0.99.99")) > 0);
    }

    // ---------- 2. 面板同时展示两个版本，且不混为一个 ----------
    void panelShowsBothVersions()
    {
        VersionPanel panel(QStringLiteral("0.5.0"), QStringLiteral("0.6.161"), QStringLiteral("1.0"));
        QCOMPARE(panel.guiVersionText(), QStringLiteral("Flare Client v0.5.0"));
        QCOMPARE(panel.kernelVersionText(), QStringLiteral("flare v0.6.161（协议 1.0）"));
        // 两者必须是不同控件、不同文案（不可拼成单一版本号）
        QVERIFY(panel.guiVersionText() != panel.kernelVersionText());
    }

    void panelHandlesMissingProtocol()
    {
        VersionPanel panel(QStringLiteral("0.5.0"), QStringLiteral("0.6.161"), QString());
        QCOMPARE(panel.kernelVersionText(), QStringLiteral("flare v0.6.161"));
    }

    void panelHandlesDisconnectedKernel()
    {
        VersionPanel panel(QStringLiteral("0.5.0"), QString(), QString());
        QVERIFY(panel.kernelVersionText().contains(QStringLiteral("flare")));
        QVERIFY(panel.kernelVersionText().contains(QStringLiteral("未连接")));
        // 内核未连接 → 内核检查按钮禁用，客户端检查按钮仍可用
        auto *kernelBtn = panel.findChild<QPushButton *>(QStringLiteral("kernelCheckButton"));
        auto *guiBtn = panel.findChild<QPushButton *>(QStringLiteral("guiCheckButton"));
        QVERIFY(kernelBtn);
        QVERIFY(guiBtn);
        QVERIFY(!kernelBtn->isEnabled());
        QVERIFY(guiBtn->isEnabled());
    }

    // ---------- 3. 两条更新通道互相独立 ----------
    void channelsAreIndependent()
    {
        VersionPanel panel(QStringLiteral("0.5.0"), QStringLiteral("0.6.161"), QStringLiteral("1.0"));
        auto *guiStatus = panel.findChild<QLabel *>(QStringLiteral("guiStatusLabel"));
        auto *kernelStatus = panel.findChild<QLabel *>(QStringLiteral("kernelStatusLabel"));
        QVERIFY(guiStatus);
        QVERIFY(kernelStatus);
        QCOMPARE(guiStatus->text(), QStringLiteral("未检查"));
        QCOMPARE(kernelStatus->text(), QStringLiteral("未检查"));

        // 触发内核通道（无内核版本时不发网络请求，只改自己那行）
        VersionPanel offline(QStringLiteral("0.5.0"), QString(), QString());
        auto *offGui = offline.findChild<QLabel *>(QStringLiteral("guiStatusLabel"));
        auto *offKernel = offline.findChild<QLabel *>(QStringLiteral("kernelStatusLabel"));
        QVERIFY(offGui);
        QVERIFY(offKernel);
        QMetaObject::invokeMethod(&offline, "checkKernelUpdates", Qt::DirectConnection);
        QVERIFY(offKernel->text().contains(QStringLiteral("未连接")));
        // 关键断言：内核通道失败不污染客户端通道
        QCOMPARE(offGui->text(), QStringLiteral("未检查"));
    }

    void checkerInstancesAreSeparateObjects()
    {
        VersionPanel panel(QStringLiteral("0.5.0"), QStringLiteral("0.6.161"), QStringLiteral("1.0"));
        const auto checkers = panel.findChildren<UpdateChecker *>();
        QCOMPARE(checkers.size(), 2); // 客户端一个、内核一个，绝不共用
    }
};

QTEST_MAIN(VersionTest)
#include "version_test.moc"
