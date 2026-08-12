#include "mainwindow.h"
#include "chatwidget.h"
#include "sessionlistwidget.h"

#include <QStatusBar>
#include <QSplitter>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_engine(std::make_unique<EngineBridge>(this))
{
    setWindowTitle(QStringLiteral("Flare"));
    resize(980, 640);
    setMinimumSize(820, 600);

    // ---- 浅色白底紫配主题（用户 UI 铁律）----
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xff, 0xff, 0xff));
    pal.setColor(QPalette::WindowText, QColor(0x33, 0x33, 0x33));
    pal.setColor(QPalette::Base, QColor(0xfa, 0xfa, 0xfe));
    pal.setColor(QPalette::AlternateBase, QColor(0xf4, 0xf2, 0xff));
    pal.setColor(QPalette::Highlight, QColor(0x6d, 0x4a, 0xff));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(pal);

    // ---- 左右布局：会话侧边栏 | 聊天面板 ----
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_sessions = new SessionListWidget(m_engine.get(), splitter);
    m_chat = new ChatWidget(m_engine.get(), splitter);
    splitter->addWidget(m_sessions);
    splitter->addWidget(m_chat);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(0, false);
    setCentralWidget(splitter);

    // 侧边栏选中会话 → 聊天面板切换目标会话
    connect(m_sessions, &SessionListWidget::sessionSelected,
            m_chat, &ChatWidget::setSession);

    // 启动 flare server（后台，不阻塞 UI）
    if (m_engine->start()) {
        statusBar()->showMessage(QStringLiteral("Flare 引擎已连接"));
        m_engine->queryVersion();
        m_sessions->refresh();
    } else {
        statusBar()->showMessage(QStringLiteral("⚠️ Flare 引擎启动失败（请确认 flare 已安装）"));
    }
}

MainWindow::~MainWindow() = default;
