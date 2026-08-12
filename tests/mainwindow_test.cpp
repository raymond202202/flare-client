#include <QtTest>
#include <QApplication>
#include <QPalette>
#include <QJsonObject>
#include <QSignalSpy>
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
};

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
    // 高亮色 = 紫色系 #6d4aff（主色）
    QColor hl = w.palette().color(QPalette::Highlight);
    QVERIFY2(hl.blue() > hl.red(), "Highlight 应为紫色系（蓝>红）");
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
                                 {"apiKey", "sk-this-is-a-secret-please-never-print"}}}}}};
    w.onEngineEvent(ev);
    QCOMPARE(w.modelInfoText(), QString("qwen2.5:7b · ollama"));
    QVERIFY2(!w.modelInfoText().contains("sk-this-is-a-secret"),
             "模型信息展示不得包含 apiKey 值");
}

QTEST_MAIN(MainWindowTest)
#include "mainwindow_test.moc"
