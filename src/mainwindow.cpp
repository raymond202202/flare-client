#include "mainwindow.h"
#include "chatwidget.h"
#include "sessionlistwidget.h"
#include "memorypanel.h"

#include <QStatusBar>
#include <QSplitter>
#include <QHBoxLayout>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonObject>

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
    // 侧边栏「记忆」→ 弹出记忆面板
    connect(m_sessions, &SessionListWidget::memoryRequested,
            this, &MainWindow::openMemoryPanel);
    // M3-6: 引擎事件 → 解析 models 展示模型信息
    connect(m_engine.get(), &EngineBridge::eventReceived,
            this, &MainWindow::onEngineEvent);

    // M3-6: 状态栏右侧模型信息标签（只显示 model/provider）
    m_modelLabel = new QLabel(QStringLiteral("🤖 模型加载中…"), this);
    m_modelLabel->setStyleSheet(QStringLiteral("color:#6d4aff; padding:0 8px;"));
    statusBar()->addPermanentWidget(m_modelLabel);

    // 启动 flare server（后台，不阻塞 UI）
    if (m_engine->start()) {
        statusBar()->showMessage(QStringLiteral("Flare 引擎已连接"));
        m_engine->queryVersion();
        m_engine->queryModels();
        m_sessions->refresh();
    } else {
        statusBar()->showMessage(QStringLiteral("⚠️ Flare 引擎启动失败（请确认 flare 已安装）"));
    }
}

// M3-6: models 事件 → 提取 configured.main 的 model + provider，仅展示这两项
// 安全：绝不读取/显示 apiKey、baseURL 等敏感字段
void MainWindow::onEngineEvent(const QJsonObject &obj)
{
    if (obj.value(QStringLiteral("type")).toString() != QLatin1String(FlareEvent::Models))
        return;
    const QJsonObject configured = obj.value(QStringLiteral("configured")).toObject();
    const QJsonObject main = configured.value(QStringLiteral("main")).toObject();
    const QString model = main.value(QStringLiteral("model")).toString();
    const QString provider = main.value(QStringLiteral("provider")).toString();

    QString info;
    if (!model.isEmpty()) {
        info = model;
        if (!provider.isEmpty())
            info += QStringLiteral(" · ") + provider;
    }
    if (info.isEmpty()) {
        info = QStringLiteral("模型信息不可用");
    } else {
        m_modelInfo = info;
        if (m_modelLabel)
            m_modelLabel->setText(QStringLiteral("🤖 ") + info);
    }
}

void MainWindow::openMemoryPanel()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("记忆"));
    dlg->setModal(false);
    dlg->resize(420, 480);
    auto *panel = new MemoryPanel(m_engine.get(), dlg);
    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(panel);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    panel->refresh();
}

MainWindow::~MainWindow() = default;
