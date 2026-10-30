#include "mainwindow.h"
#include "chatwidget.h"
#include "sessionlistwidget.h"
#include "memorypanel.h"
#include "skillpanel.h"
#include "settingspanel.h"
#include "versionpanel.h"

#include <QStatusBar>
#include <QSplitter>
#include <QHBoxLayout>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QJsonObject>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QStyle>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>

// 客户端自身版本：编译期由 CMake 的 FLARE_CLIENT_VERSION 注入（source point = CMakeLists.txt VERSION）
#ifndef FLARE_CLIENT_VERSION
#define FLARE_CLIENT_VERSION "0.0.0"
#endif

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
    // M6-2 侧边栏「设置」→ 弹出设置面板
    connect(m_sessions, &SessionListWidget::settingsRequested,
            this, &MainWindow::openSettingsPanel);
    // M3-6: 引擎事件 → 解析 models 展示模型信息
    connect(m_engine.get(), &EngineBridge::eventReceived,
            this, &MainWindow::onEngineEvent);
    // M8: 会话首条消息 → 侧边栏别名，让列表名字能看出会话内容
    connect(m_chat, &ChatWidget::firstMessageSent,
            m_sessions, &SessionListWidget::setLocalAlias);

    // M3-6: 状态栏右侧模型信息标签（只显示 model/provider）
    m_modelLabel = new QLabel(QStringLiteral("🤖 模型加载中…"), this);
    m_modelLabel->setStyleSheet(QStringLiteral("color:#f97316; padding:0 8px;"));
    statusBar()->addPermanentWidget(m_modelLabel);

    // 版本入口：状态栏常显「客户端 vX · 内核 flare vY」，点击打开版本与更新面板
    // 两个版本号来源不同（编译期 vs 运行时 version 帧），故不可拼成单一版本号
    m_versionButton = new QPushButton(this);
    m_versionButton->setObjectName(QStringLiteral("versionButton"));
    m_versionButton->setText(versionText());
    m_versionButton->setCursor(Qt::PointingHandCursor);
    m_versionButton->setToolTip(QStringLiteral("查看版本并分别检查客户端 / 内核更新"));
    m_versionButton->setFlat(true);
    m_versionButton->setFocusPolicy(Qt::NoFocus);
    m_versionButton->setStyleSheet(QStringLiteral(
        "QPushButton { border:none; color:#b45309; background:transparent;"
        "              font-size:12px; padding:0 6px; }"
        "QPushButton:hover { color:#f97316; text-decoration:underline; }"
        "QPushButton:pressed { color:#c2410c; }"));
    connect(m_versionButton, &QPushButton::clicked, this, &MainWindow::openVersionPanel);
    statusBar()->addPermanentWidget(m_versionButton);

    // M8 工作目录：引擎工具沙箱根硬编码 os.Getwd()（internal/tools 里没有 cwd 参数），
    // 所以把用户选的目录当成引擎进程的工作目录传下去；换目录后重启引擎即生效
    const QString savedWorkdir = QSettings(settingsFilePath(), QSettings::IniFormat)
                                     .value(QStringLiteral("engine/workdir"))
                                     .toString();
    if (!savedWorkdir.isEmpty() && QFileInfo(savedWorkdir).isDir())
        m_engine->setWorkingDirectory(savedWorkdir);

    m_workdirButton = new QPushButton(this);
    m_workdirButton->setObjectName(QStringLiteral("workdirButton"));
    m_workdirButton->setText(workdirText());
    m_workdirButton->setCursor(Qt::PointingHandCursor);
    m_workdirButton->setFlat(true);
    m_workdirButton->setFocusPolicy(Qt::NoFocus);
    m_workdirButton->setStyleSheet(QStringLiteral(
        "QPushButton { border:none; color:#b45309; background:transparent;"
        "              font-size:12px; padding:0 6px; }"
        "QPushButton:hover { color:#f97316; text-decoration:underline; }"
        "QPushButton:pressed { color:#c2410c; }"));
    m_workdirButton->setToolTip(
        m_engine->workingDirectory().isEmpty()
            ? QStringLiteral("点击选择 Flare 的工作目录（Flare 读写本地文件的根目录）")
            : QStringLiteral("当前工作目录：%1\n点击切换（会重启引擎）")
                  .arg(m_engine->workingDirectory()));
    connect(m_workdirButton, &QPushButton::clicked,
            this, &MainWindow::chooseWorkingDirectory);
    statusBar()->addPermanentWidget(m_workdirButton);

    // M8 托盘：关窗口不退出（用户反馈：误关一次会话上下文就没了）
    setupTray();
    checkApiKeyConfigured();   // 没填 key 就在状态栏提醒，别让用户对着空对话框发呆

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
    const QString type = obj.value(QStringLiteral("type")).toString();

    // version 帧：内核名字固定 flare，版本与协议号来自内核自报
    if (type == QLatin1String(FlareEvent::Version)) {
        m_kernelVersion = obj.value(QStringLiteral("engine")).toString();
        m_protocolVersion = obj.value(QStringLiteral("protocol")).toString();
        if (m_versionButton)
            m_versionButton->setText(versionText());
        return;
    }

    if (type != QLatin1String(FlareEvent::Models))
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

void MainWindow::openSettingsPanel()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("设置"));
    dlg->setModal(true);
    dlg->resize(440, 320);
    // M6-4: exec() 模态结束后自动销毁，避免每次打开泄漏 QDialog
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    auto *panel = new SettingsPanel(dlg);
    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(panel);
    dlg->exec();
    // 填完 key 回来，状态栏提示应该自己消失
    const bool hadHint = (m_keyHintButton != nullptr);
    checkApiKeyConfigured();
    if (hadHint && !m_keyHintButton)
        statusBar()->showMessage(QStringLiteral("API Key 已保存，下一轮对话即生效"), 6000);
}

// 装了包、打开界面、发消息却没有任何反应 —— 首次使用最容易卡在这一步。
// 与其等引擎抛错，不如在状态栏常驻一个能点的提示。
bool MainWindow::envHasApiKey(const QString &envText)
{
    const QStringList lines = envText.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))
            || !line.contains(QLatin1Char('=')))
            continue;
        // 与引擎 internal/config LoadDotEnv 的解析保持一致：
        // 容许 "export " 前缀、容许单/双引号包裹
        QString keyName = line.section(QLatin1Char('='), 0, 0).trimmed();
        if (keyName.startsWith(QLatin1String("export ")))
            keyName = keyName.mid(7).trimmed();
        QString value = line.section(QLatin1Char('='), 1).trimmed();
        if (value.size() >= 2
            && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))))
            value = value.mid(1, value.size() - 2);
        if (value.isEmpty())
            continue;
        // DEEPSEEK_API_KEY / OPENAI_API_KEY / LLM_API_KEY 都算已配置；
        // 形如 DEEPSEEK_API_KEY= 的空值不算（模板就是这样，不能当成已配置）
        if (keyName.endsWith(QLatin1String("API_KEY")))
            return true;
    }
    return false;
}

void MainWindow::checkApiKeyConfigured()
{
    QString envText;
    QFile f(QDir::homePath() + QStringLiteral("/.flare/.env"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        envText = QString::fromUtf8(f.readAll());
        f.close();
    }
    const bool hasKey = envHasApiKey(envText);

    if (hasKey) {
        if (m_keyHintButton) {
            statusBar()->removeWidget(m_keyHintButton);
            m_keyHintButton->deleteLater();
            m_keyHintButton = nullptr;
        }
        return;
    }
    if (m_keyHintButton)
        return; // 提示已在，不重复加

    m_keyHintButton = new QPushButton(QStringLiteral("⚠️ 还没填 API Key · 点这里设置"), this);
    m_keyHintButton->setObjectName(QStringLiteral("keyHintButton"));
    m_keyHintButton->setCursor(Qt::PointingHandCursor);
    m_keyHintButton->setFlat(true);
    m_keyHintButton->setFocusPolicy(Qt::NoFocus);
    m_keyHintButton->setToolTip(
        QStringLiteral("填入 API Key 后才能对话。配置写在 ~/.flare/.env，与命令行版共用。"));
    m_keyHintButton->setStyleSheet(QStringLiteral(
        "QPushButton { border:1px solid #fdba74; border-radius:10px;"
        "              background:#fff7ed; color:#c2410c; font-size:12px;"
        "              padding:2px 10px; }"
        "QPushButton:hover { background:#ffedd0; color:#f97316; }"));
    connect(m_keyHintButton, &QPushButton::clicked, this, &MainWindow::openSettingsPanel);
    statusBar()->addPermanentWidget(m_keyHintButton);
}

// 状态栏版本文本：客户端版本（编译期）+ 内核名字与版本（运行时 version 帧）
QString MainWindow::versionText() const
{
    const QString gui = QCoreApplication::applicationVersion().isEmpty()
                            ? QStringLiteral(FLARE_CLIENT_VERSION)
                            : QCoreApplication::applicationVersion();
    QString text = QStringLiteral("客户端 v%1").arg(gui);
    if (m_kernelVersion.isEmpty())
        text += QStringLiteral(" · 内核 flare 未连接");
    else
        text += QStringLiteral(" · 内核 flare v%1").arg(m_kernelVersion);
    return text;
}

// 版本与更新面板：GUI / 内核各自一行、各自一个「检查更新」，互不影响
void MainWindow::openVersionPanel()
{
    const QString gui = QCoreApplication::applicationVersion().isEmpty()
                            ? QStringLiteral(FLARE_CLIENT_VERSION)
                            : QCoreApplication::applicationVersion();
    auto *dlg = new VersionPanel(gui, m_kernelVersion, m_protocolVersion, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

// ---------------- M8: 工作目录 / 托盘 / 关闭行为 ----------------

// 与 SessionListWidget 的 pinnedSettings() 指向同一个 ini：
// QSettings(IniFormat, UserScope, "flare", "flare-client") → ~/.config/flare/flare-client.ini
QString MainWindow::settingsFilePath() const
{
    static const QString path =
        QSettings(QSettings::IniFormat, QSettings::UserScope,
                  QStringLiteral("flare"), QStringLiteral("flare-client"))
            .fileName();
    return path;
}

QString MainWindow::workdirText() const
{
    const QString dir = m_engine->workingDirectory();
    if (dir.isEmpty())
        return QStringLiteral("📂 工作目录：默认");
    QString name = QDir(dir).dirName();
    if (name.isEmpty())
        name = dir;
    return QStringLiteral("📂 %1").arg(name);
}

void MainWindow::chooseWorkingDirectory()
{
    const QString start = m_engine->workingDirectory().isEmpty()
                              ? QDir::homePath()
                              : m_engine->workingDirectory();
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择 Flare 的工作目录（Flare 读写本地文件的根目录）"),
        start);
    if (dir.isEmpty())
        return;

    QSettings(QSettings::IniFormat, QSettings::UserScope,
              QStringLiteral("flare"), QStringLiteral("flare-client"))
        .setValue(QStringLiteral("engine/workdir"), dir);
    m_engine->setWorkingDirectory(dir);

    if (m_workdirButton) {
        m_workdirButton->setText(workdirText());
        m_workdirButton->setToolTip(
            QStringLiteral("当前工作目录：%1\n点击切换（会重启引擎）").arg(dir));
    }

    // 引擎工具的沙箱根是进程 cwd，运行中改不了 → 重启一次
    m_engine->stop();
    if (m_engine->start()) {
        statusBar()->showMessage(
            QStringLiteral("工作目录已切换到 %1，引擎已重启").arg(dir), 8000);
        m_engine->queryVersion();
        m_engine->queryModels();
        m_sessions->refresh();
    } else {
        statusBar()->showMessage(
            QStringLiteral("⚠️ 切换工作目录后引擎重启失败"), 8000);
    }
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return; // 无托盘环境（部分 Wayland 会话）→ closeEvent 退化为最小化

    QIcon trayIcon = windowIcon();
    if (trayIcon.isNull())
        trayIcon = QIcon::fromTheme(QStringLiteral("flare-client"));
    if (trayIcon.isNull())
        trayIcon = style()->standardIcon(QStyle::SP_ComputerIcon);

    m_tray = new QSystemTrayIcon(trayIcon, this);
    m_tray->setToolTip(QStringLiteral("Flare"));

    auto *menu = new QMenu(this);
    QAction *showAct = menu->addAction(QStringLiteral("显示主窗口"));
    connect(showAct, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    menu->addSeparator();
    QAction *quitAct = menu->addAction(QStringLiteral("退出 Flare"));
    connect(quitAct, &QAction::triggered, this, [this]() {
        m_forceQuit = true;
        qApp->quit();
    });
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick) {
                    showNormal();
                    raise();
                    activateWindow();
                }
            });
    m_tray->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 托盘「退出」才真退；否则关窗口只是收起来（用户反馈：误关一次上下文就没了）
    if (m_forceQuit) {
        event->accept();
        return;
    }
    event->ignore();
    if (m_tray && m_tray->isVisible()) {
        hide();
        m_tray->showMessage(
            QStringLiteral("Flare 还在运行"),
            QStringLiteral("窗口已收进托盘。点托盘图标可恢复，右键「退出 Flare」才彻底关闭。"),
            QSystemTrayIcon::Information, 4000);
    } else {
        showMinimized();
        statusBar()->showMessage(
            QStringLiteral("Flare 仍在运行（窗口已最小化）"), 5000);
    }
}

MainWindow::~MainWindow() = default;
