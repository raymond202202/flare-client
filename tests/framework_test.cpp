#include <QtTest>
#include <QString>

// 示例测试：验证测试框架本身工作（TDD 起点）
class FrameworkTest : public QObject
{
    Q_OBJECT

private slots:
    void frameworkWorks();
    void stringTrims();
};

void FrameworkTest::frameworkWorks()
{
    QVERIFY(true);  // 测试框架能跑
}

void FrameworkTest::stringTrims()
{
    QString s = "  flare  ";
    QCOMPARE(s.trimmed(), QString("flare"));
}

QTEST_MAIN(FrameworkTest)
#include "framework_test.moc"
