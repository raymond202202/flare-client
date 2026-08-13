#include <QtTest>
#include <QSignalSpy>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include "settingspanel.h"

// M6-2: 设置面板测试 —— .env 读取/写入/权限
class SettingsPanelTest : public QObject
{
    Q_OBJECT

private:
    QString m_origHome;

private slots:
    void initTestCase()
    {
        m_origHome = qEnvironmentVariable("HOME");
    }
    void cleanupTestCase()
    {
        qputenv("HOME", m_origHome.toUtf8());
    }

    void init()
    {
        QTemporaryDir dir;
        qputenv("HOME", QByteArray(dir.path().toUtf8()));
        QDir().mkpath(dir.path() + "/.flare");
    }

    void envReadsExistingKey()
    {
        QTemporaryDir dir;
        qputenv("HOME", QByteArray(dir.path().toUtf8()));
        QDir().mkpath(dir.path() + "/.flare");
        {
            QFile f(dir.path() + "/.flare/.env");
            f.open(QIODevice::WriteOnly);
            f.write("DEEPSEEK_API_KEY=sk-test-123\nDEFAULT_MODEL=deepseek-chat\nVISION_MODEL=qwen2.5vl:3b\n");
            f.close();
        }
        SettingsPanel p;
        QCOMPARE(p.deepseekKeyEdit()->text(), QStringLiteral("sk-test-123"));
        QCOMPARE(p.modelEdit()->text(), QStringLiteral("deepseek-chat"));
    }

    // M6-4: model 留空 → 不写 DEFAULT_MODEL 空值行（避免破坏 CLI 配置）
    void saveWithEmptyModelRemovesDefaultModel()
    {
        QTemporaryDir dir;
        qputenv("HOME", QByteArray(dir.path().toUtf8()));
        QDir().mkpath(dir.path() + "/.flare");
        {
            QFile f(dir.path() + "/.flare/.env");
            f.open(QIODevice::WriteOnly);
            f.write("DEEPSEEK_API_KEY=old\nDEFAULT_MODEL=deepseek-chat\nVISION_MODEL=qwen2.5vl:3b\n");
            f.close();
        }
        SettingsPanel p;
        p.deepseekKeyEdit()->setText(QStringLiteral("sk-new"));
        p.modelEdit()->setText(QString()); // 用户清空模型 → 走引擎默认
        p.save();
        QFile f(dir.path() + "/.flare/.env");
        f.open(QIODevice::ReadOnly);
        const QString content = QString::fromUtf8(f.readAll());
        f.close();
        QVERIFY2(content.contains(QStringLiteral("DEEPSEEK_API_KEY=sk-new")), qPrintable(content));
        QVERIFY2(!content.contains(QStringLiteral("DEFAULT_MODEL=")),
                 "model 留空时不得写入 DEFAULT_MODEL 空值行");
        QVERIFY2(content.contains(QStringLiteral("VISION_MODEL=qwen2.5vl:3b")),
                 "其他变量应保留");
    }

    void saveWritesAndChmod600()
    {
        QTemporaryDir dir;
        qputenv("HOME", QByteArray(dir.path().toUtf8()));
        QDir().mkpath(dir.path() + "/.flare");
        SettingsPanel p;
        p.deepseekKeyEdit()->setText(QStringLiteral("sk-new-key"));
        p.modelEdit()->setText(QStringLiteral("deepseek-chat"));
        p.save();
        QFile f(dir.path() + "/.flare/.env");
        QVERIFY(f.exists());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(f.readAll());
        f.close();
        QVERIFY2(content.contains(QStringLiteral("DEEPSEEK_API_KEY=sk-new-key")),
                 "保存的 key 应写入 .env");
        const QFile::Permissions perm = f.permissions();
        QVERIFY2(!(perm & QFileDevice::ReadGroup) && !(perm & QFileDevice::ReadOther),
                 "权限应为 600（组/他人不可读）");
    }

    void saveKeepsOtherVars()
    {
        QTemporaryDir dir;
        qputenv("HOME", QByteArray(dir.path().toUtf8()));
        QDir().mkpath(dir.path() + "/.flare");
        {
            QFile f(dir.path() + "/.flare/.env");
            f.open(QIODevice::WriteOnly);
            f.write("DEEPSEEK_API_KEY=old\nVISION_MODEL=qwen2.5vl:3b\nVISION_BASE_URL=https://x\n");
            f.close();
        }
        SettingsPanel p;
        p.deepseekKeyEdit()->setText(QStringLiteral("sk-new"));
        p.modelEdit()->setText(QStringLiteral("deepseek-chat"));
        p.save();
        QFile f(dir.path() + "/.flare/.env");
        f.open(QIODevice::ReadOnly);
        const QString content = QString::fromUtf8(f.readAll());
        f.close();
        QVERIFY2(content.contains(QStringLiteral("VISION_MODEL=qwen2.5vl:3b")),
                 "其他变量（VISION_MODEL）应保留");
        QVERIFY2(content.contains(QStringLiteral("DEEPSEEK_API_KEY=sk-new")),
                 "key 应更新为新值");
    }
};

QTEST_MAIN(SettingsPanelTest)
#include "settingspanel_test.moc"
