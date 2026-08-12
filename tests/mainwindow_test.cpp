#include <QtTest>
#include <QApplication>
#include <QPalette>
#include "../src/mainwindow.h"

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

QTEST_MAIN(MainWindowTest)
#include "mainwindow_test.moc"
