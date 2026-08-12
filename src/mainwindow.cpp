#include "mainwindow.h"
#include "chatwidget.h"

#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_engine(std::make_unique<EngineBridge>(this))
{
    setWindowTitle(QStringLiteral("Flare"));
    resize(900, 640);
    setMinimumSize(800, 600);

    // ---- 浅色白底紫配主题（用户 UI 铁律）----
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xff, 0xff, 0xff));
    pal.setColor(QPalette::WindowText, QColor(0x33, 0x33, 0x33));
    pal.setColor(QPalette::Base, QColor(0xfa, 0xfa, 0xfe));
    pal.setColor(QPalette::AlternateBase, QColor(0xf4, 0xf2, 0xff));
    pal.setColor(QPalette::Highlight, QColor(0x6d, 0x4a, 0xff));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(pal);

    // 聊天面板
    m_chat = new ChatWidget(m_engine.get(), this);
    setCentralWidget(m_chat);

    // 启动 flare server（后台，不阻塞 UI）
    if (m_engine->start()) {
        statusBar()->showMessage(QStringLiteral("Flare 引擎已连接"));
        m_engine->queryVersion();
    } else {
        statusBar()->showMessage(QStringLiteral("⚠️ Flare 引擎启动失败（请确认 flare 已安装）"));
    }
}

MainWindow::~MainWindow() = default;
