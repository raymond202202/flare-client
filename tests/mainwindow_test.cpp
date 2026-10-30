#include <QtTest>
#include <QApplication>
#include <QPalette>
#include <QJsonObject>
#include <QSignalSpy>
#include <QPushButton>
#include <QStatusBar>
#include "../src/mainwindow.h"
#include "engine_bridge.h"

// 主窗口冒烟测试（M0 验收项）
// 验证：窗口能创建、标题正确、最小尺寸达标、浅色白底紫配主题生效
class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void windowCreates();
    void windowTitleCorrect();
    void windowSizeMeetsMinimum();
    void themeIsLightPurple();
    void modelsEventShowsModelInfo();
    void modelsEventIgnoresSensitiveFields();
    void engineStatusButtonOffersRestart();
};

// 引擎断了必须能原地重启：状态栏常驻入口，断开态的文案要写清「点击重启」，
// 否则用户看到「已断开」只会去重开客户端，正在读的对话就没了。
void MainWindowTest::engineStatusButtonOffersRestart()
{
    MainWindow w;
    auto *btn = w.findChild<QPushButton *>(QStringLiteral("engineStatusButton"));
    QVERIFY2(btn, "状态栏必须有引擎状态入口（objectName=engineStatusButton）");
    QVERIFY2(!btn->text().isEmpty(), "引擎状态不能为空字符串");
    if (btn->text().contains(QStringLiteral("断开"))) {
        QVERIFY2(btn->text().contains(QStringLiteral("重启")),
                 qPrintable(QStringLiteral("断开态必须提示怎么恢复，实际：") + btn->text()));
        QVERIFY(btn->cursor().shape() == Qt::PointingHandCursor);
        QVERIFY2(!btn->toolTip().isEmpty(), "断开态要有 tooltip 说明点了会发生什么");
    } else {
        QVERIFY2(btn->text().contains(QStringLiteral("已连接")), qPrintable(btn->text()));
    }
}

void MainWindowTest::windowCreates()
{
    MainWindow w;
    QVERIFY(w.windowTitle() == QString("Flare"));
}

void MainWindowTest::windowTitleCorrect()
{
    MainWindow w;
    QCOMPARE(w.windowTitle(), QString("Flare"));
}

void MainWindowTest::windowSizeMeetsMinimum()
{
    MainWindow w;
    QVERIFY(w.minimumWidth() >= 800);
    QVERIFY(w.minimumHeight() >= 600);
    QVERIFY(w.width() >= w.minimumWidth());
    QVERIFY(w.height() >= w.minimumHeight());
}

void MainWindowTest::themeIsLightPurple()
{
    MainWindow w;
    // 窗口背景 = 白色
    QColor win = w.palette().color(QPalette::Window);
    QVERIFY2(win.lightness() > 240, "Window 背景应为白色");
    // 高亮色 = 火焰橙 #f97316（M5-1 主色，红>蓝）
    QColor hl = w.palette().color(QPalette::Highlight);
    QVERIFY2(hl.red() > hl.blue(), "Highlight 应为火焰橙色（红>蓝）");
    QVERIFY2(hl.value() > 100, "Highlight 饱和度足够");
}

// M3-6: models 事件 → 状态栏展示 model + provider
void MainWindowTest::modelsEventShowsModelInfo()
{
    MainWindow w;
    QJsonObject ev{
        {"type", "models"},
        {"configured", QJsonObject{
            {"main", QJsonObject{
                {"model", "deepseek-chat"},
                {"provider", "deepseek"},
                {"baseURL", "https://example.invalid/v1"}, // 不应展示
                {"hasApiKey", true}}}}}};
    w.onEngineEvent(ev);
    QCOMPARE(w.modelInfoText(), QString("deepseek-chat · deepseek"));
}

// M3-6: models 事件空字段 → 显示不可用；事件里即便带 key 也不进入展示文本
void MainWindowTest::modelsEventIgnoresSensitiveFields()
{
    MainWindow w;
    QJsonObject ev{
        {"type", "models"},
        {"configured", QJsonObject{
            {"main", QJsonObject{{"model", "qwen2.5:7b"},
                                 {"provider", "ollama"},
                                 {"apiKey", "dummy-api-key-for-ui-test"}}}}}};
    w.onEngineEvent(ev);
    QCOMPARE(w.modelInfoText(), QString("qwen2.5:7b · ollama"));
    QVERIFY2(!w.modelInfoText().contains("dummy-api-key-for-ui-test"),
             "模型信息展示不得包含 apiKey 值");
}

QTEST_MAIN(MainWindowTest)
#include "mainwindow_test.moc"
