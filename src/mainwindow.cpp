#include "mainwindow.h"
#include "chatwidget.h"
#include "sessionlistwidget.h"
#include "memorypanel.h"
#include "skillpanel.h"

#include <QStatusBar>
#include <QSplitter>
#include <QHBoxLayout>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonObject>
#include <QIcon>
#include <QFile>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_engine(std::make_unique<EngineBridge>(this))
{
    setWindowTitle(QStringLiteral("Flare"));
    resize(980, 640);
    setMinimumSize(820, 600);

    // 用户铁律：无菜单栏（显式置空，避免任何平台默认菜单）
    setMenuBar(nullptr);

    // 窗口图标（RPM 安装路径 /usr/share/icons/hicolor/scalable/apps/，
    // 便携安装版 <appDir>/flare-icon.svg（M6-1：任意用户路径可用），
    // 开发时回退 packaging/flare-client.svg）
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList iconCandidates = {
        QStringLiteral("/usr/share/icons/hicolor/scalable/apps/flare-client.svg"),
        appDir + QStringLiteral("/flare-icon.svg"),
        QStringLiteral("packaging/flare-client.svg"),
    };
    for (const QString &path : iconCandidates) {
        if (QFile::exists(path)) {
            setWindowIcon(QIcon(path));
            break;
        }
    }

    // ---- 火焰活力色系主题（用户铁律：红橙黄火焰色）----
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xff, 0xff, 0xff));
    pal.setColor(QPalette::WindowText, QColor(0x4a, 0x2e, 0x0d));
    pal.setColor(QPalette::Base, QColor(0xff, 0xfb, 0xf0));
    pal.setColor(QPalette::AlternateBase, QColor(0xff, 0xf3, 0xd6));
    pal.setColor(QPalette::Highlight, QColor(0xf9, 0x73, 0x16));
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
    // M5-3 侧边栏「技能」→ 弹出技能面板
    connect(m_sessions, &SessionListWidget::skillRequested,
            this, &MainWindow::openSkillPanel);
    // M3-6: 引擎事件 → 解析 models 展示模型信息
    connect(m_engine.get(), &EngineBridge::eventReceived,
            this, &MainWindow::onEngineEvent);

    // M3-6: 状态栏右侧模型信息标签（只显示 model/provider）
    m_modelLabel = new QLabel(QStringLiteral("🤖 模型加载中…"), this);
    m_modelLabel->setStyleSheet(QStringLiteral("color:#f97316; padding:0 8px;"));
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

void MainWindow::openSkillPanel()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("技能与工具"));
    dlg->setModal(false);
    dlg->resize(420, 480);
    auto *panel = new SkillPanel(m_engine.get(), dlg);
    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(panel);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    panel->refresh();
}

MainWindow::~MainWindow() = default;
