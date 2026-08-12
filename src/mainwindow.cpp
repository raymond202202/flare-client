#include "mainwindow.h"

#include <QApplication>
#include <QStatusBar>
#include <QLabel>
#include <QStyle>
#include <QPalette>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Flare"));
    resize(900, 640);
    setMinimumSize(800, 600);

    // ---- 浅色白底紫配主题（用户 UI 铁律）----
    // 主色 #6d4aff（紫），背景白色，文本深灰
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xff, 0xff, 0xff));        // 白底
    pal.setColor(QPalette::WindowText, QColor(0x33, 0x33, 0x33));    // 深灰文本
    pal.setColor(QPalette::Base, QColor(0xfa, 0xfa, 0xfe));          // 输入区微紫白
    pal.setColor(QPalette::AlternateBase, QColor(0xf4, 0xf2, 0xff)); // 交替行淡紫
    pal.setColor(QPalette::Highlight, QColor(0x6d, 0x4a, 0xff));     // 选中紫
    pal.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(pal);

    // 占位状态栏（后续迭代：会话状态 / 引擎状态）
    statusBar()->showMessage(QStringLiteral("Flare 客户端 · M0 骨架"));
}
