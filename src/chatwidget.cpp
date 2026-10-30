#include "chatwidget.h"
#include "markdownlite.h"

#include <QLabel>
#include <QJsonValue>
#include <QDateTime>
#include <QTextCursor>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QColor>
#include <QFont>
#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QImage>
#include <QImageReader>
#include <QIcon>
#include <QPixmap>
#include <QKeyEvent>
#include <QKeySequence>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QUrl>
#include "flametheme.h"

namespace {
// 消息区样式（常态 / 拖放高亮共用一个来源，避免两处色值漂移）
const char *kOutputStyle =
    "QTextEdit { background:#fffbf0; border:1px solid #fde6bf; border-radius:8px;"
    " font-size:14px; padding:8px; }";
const char *kOutputStyleDrop =
    "QTextEdit { background:#fff7ed; border:2px solid #f97316; border-radius:8px;"
    " font-size:14px; padding:8px; }";

// 一次能带几个附件：从文件管理器框一片文件拖进来是常事，几百个会把输入区顶到看不见
constexpr int kMaxAttachments = 10;

// 图片后缀（决定 chip 上给不给缩略图）
bool looksLikeImage(const QFileInfo &fi)
{
    static const QStringList kImageSuffix{
        QStringLiteral("png"),  QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("webp"), QStringLiteral("gif"), QStringLiteral("bmp"),
        QStringLiteral("svg"),  QStringLiteral("avif")};
    return kImageSuffix.contains(fi.suffix().toLower());
}
} // namespace

ChatWidget::ChatWidget(EngineBridge *bridge, QWidget *parent)
    : QWidget(parent)
    , m_bridge(bridge)
    , m_output(new QTextEdit(this))
    , m_input(new QLineEdit(this))
    , m_sendBtn(new QPushButton(QStringLiteral("发送"), this))
    , m_stopBtn(new QPushButton(QStringLiteral("⏹ 停止"), this))
    , m_sessionId(QStringLiteral("gui-") + QString::number(QDateTime::currentMSecsSinceEpoch()))
{
    // M10 拖放：整个聊天面板接受文件拖入（文件管理器拖文件 = 附件）
    setAcceptDrops(true);
    // objectName 是自动化验收的定位锚点：曾经 e2e 用 findChild<QLineEdit*>()
    // 盲取第一个控件，被侧边栏搜索框抢走，导致"消息没发出去"的假故障
    m_output->setObjectName(QStringLiteral("chatOutput"));
    m_input->setObjectName(QStringLiteral("chatInput"));
    m_sendBtn->setObjectName(QStringLiteral("chatSendButton"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Flare 对话"), this);
    title->setStyleSheet(QStringLiteral("font-size:16px; font-weight:600; color:#f97316;"));
    layout->addWidget(title);

    // 消息区：只读、可选中复制
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(QStringLiteral("等待对话…"));
    m_output->setStyleSheet(QString::fromLatin1(kOutputStyle));
    layout->addWidget(m_output, 1);

    // 上翻读历史时新内容到达 → 右下角浮出「↓ 新内容」，点一下跳回最新。
    // 挂在 m_output 上做绝对定位：不占布局高度，不会把输入区顶下去。
    m_jumpBtn = new QPushButton(QStringLiteral("↓ 新内容"), m_output);
    m_jumpBtn->setObjectName(QStringLiteral("jumpToLatestButton"));
    m_jumpBtn->setCursor(Qt::PointingHandCursor);
    m_jumpBtn->setFocusPolicy(Qt::NoFocus);
    m_jumpBtn->setToolTip(QStringLiteral("跳到最新消息"));
    m_jumpBtn->setStyleSheet(
        QStringLiteral("QPushButton{background:#ffffff;border:1px solid #d9c9ff;"
                       "border-radius:12px;padding:4px 12px;color:#6d4aff;font-size:12px;}"
                       "QPushButton:hover{background:#f5f0ff;border-color:#6d4aff;}"));
    m_jumpBtn->hide();
    connect(m_jumpBtn, &QPushButton::clicked, this, [this]() {
        scrollToBottom();
        if (m_jumpBtn)
            m_jumpBtn->hide();
    });
    m_output->installEventFilter(this); // 尺寸变化时重新贴到右下角
    // 用户自己滚回底部 → 提示自动消失，不需要他再去点一下。
    // valueChanged 之外还要挂 rangeChanged：追加内容会先把滚动范围撑大，
    // 这一瞬间 value 可能暂时「离底部很远」，等布局稳定后不会再发 valueChanged。
    const auto hideIfAtBottom = [this]() {
        const QScrollBar *bar = m_output->verticalScrollBar();
        if (m_jumpBtn && !m_jumpBtn->isHidden() && atLatestPosition(bar->value(), bar->maximum()))
            m_jumpBtn->hide();
    };
    connect(m_output->verticalScrollBar(), &QScrollBar::valueChanged, this, [hideIfAtBottom](int) {
        hideIfAtBottom();
    });
    connect(m_output->verticalScrollBar(), &QScrollBar::rangeChanged, this, [hideIfAtBottom](int, int) {
        hideIfAtBottom();
    });

    // M8 附件 chip 行（用户反馈：输入框不能粘贴图片 / 没有读取本地文件的入口）
    m_attachBar = new QWidget(this);
    m_attachChips = new QHBoxLayout(m_attachBar);
    m_attachChips->setContentsMargins(0, 0, 0, 0);
    m_attachChips->setSpacing(6);
    m_attachChips->addStretch(1);
    m_attachBar->setVisible(false);
    layout->addWidget(m_attachBar);

    // M12 首启起点 chip（见 chatwidget.h 注释）
    m_suggestBar = new QWidget(this);
    {
        auto *suggestRow = new QHBoxLayout(m_suggestBar);
        suggestRow->setContentsMargins(0, 0, 0, 0);
        suggestRow->setSpacing(8);
        suggestRow->addStretch(1);
        const QStringList starts{
            QStringLiteral("帮我解释这段报错"),
            QStringLiteral("用表格对比两个方案"),
            QStringLiteral("读完这个文件，告诉我重点"),
        };
        for (const QString &seed : starts) {
            auto *chip = new QPushButton(seed, m_suggestBar);
            chip->setCursor(Qt::PointingHandCursor);
            chip->setStyleSheet(QStringLiteral(
                "QPushButton { background:#fffdf9; color:#9a3412; border:1px solid #fde6bf;"
                " border-radius:13px; padding:5px 12px; font-size:12px; }"
                "QPushButton:hover { background:#ffedd0; border-color:#fdba74; }"
                "QPushButton:pressed { background:#ffe0b0; }"));
            // 只填不发：用户点完还能改，主动权留在手里
            connect(chip, &QPushButton::clicked, this, [this, seed]() {
                m_input->setText(seed);
                m_input->setFocus();
            });
            suggestRow->addWidget(chip);
        }
        suggestRow->addStretch(1);
    }
    m_suggestBar->setVisible(false);
    layout->addWidget(m_suggestBar);

    // 输入行
    auto *inputRow = new QHBoxLayout;
    m_attachBtn = new QPushButton(QStringLiteral("📎"), this);
    m_attachBtn->setToolTip(QStringLiteral("选择本地文件发给 Flare（Ctrl+V 也可粘贴图片）"));
    m_attachBtn->setCursor(Qt::PointingHandCursor);
    m_attachBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#f97316; border:1px solid #fde6bf;"
        " border-radius:8px; padding:8px 12px; font-size:15px; }"
        "QPushButton:hover { background:#ffedd0; }"
        "QPushButton:pressed { background:#ffe0b0; }"));
    inputRow->addWidget(m_attachBtn);
    m_input->setPlaceholderText(
        QStringLiteral("输入消息，Enter 发送 · Ctrl+V 粘贴图片 · 📎 选文件 · 也可直接把文件拖进来"));
    m_input->setStyleSheet(QStringLiteral(
        "QLineEdit { background:#ffffff; border:1px solid #fde6bf; border-radius:8px;"
        " padding:8px 10px; font-size:14px; }"
        "QLineEdit:focus { border-color:#f97316; }"));
    inputRow->addWidget(m_input, 1);
    m_sendBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#f97316; color:white; border:none; border-radius:8px;"
        " padding:8px 18px; font-size:14px; }"
        "QPushButton:hover { background:#e0630f; }"
        "QPushButton:pressed { background:#c04f08; }"));
    inputRow->addWidget(m_sendBtn);

    // M4-5 易用性：停止生成按钮（默认隐藏，流式输出/等待回复时显示）
    m_stopBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#ef4444; border:1px solid #fda4a4;"
        " border-radius:8px; padding:8px 14px; font-size:14px; }"
        "QPushButton:hover { background:#fef2f2; }"
        "QPushButton:pressed { background:#fde8e8; }"));
    m_stopBtn->setVisible(false);
    inputRow->addWidget(m_stopBtn);
    layout->addLayout(inputRow);

    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWidget::sendMessage);
    connect(m_input, &QLineEdit::returnPressed, this, &ChatWidget::sendMessage);
    connect(m_stopBtn, &QPushButton::clicked, this, &ChatWidget::stopGeneration);
    // M8 附件：按钮选本地文件；输入框拦 Ctrl+V 抓剪贴板图片
    connect(m_attachBtn, &QPushButton::clicked, this, &ChatWidget::pickFiles);
    m_input->installEventFilter(this);
    connect(m_bridge, &EngineBridge::eventReceived, this, &ChatWidget::onEngineEvent);
    // 引擎进程中途崩掉时界面完全没反应，用户只看到「消息发出去了但没有回复」——
    // 静默失败比报错难查得多，所以退出码非 0 就在对话区明说。
    // 注意：不能连 processError（那是引擎 stderr 的全文转发，正常运行也可能有日志）。
    connect(m_bridge, &EngineBridge::processExited, this, [this](int code) {
        if (code == 0)
            return; // 我们自己 stop() 的正常退出
        if (m_streaming)
            endStream();
        appendMessage(QStringLiteral("⚠️ 引擎异常退出"),
                      QStringLiteral("AI 内核进程退出了（退出码 %1）。"
                                     "点左下角状态栏的「⚠️ 引擎已断开」可以原地重启，"
                                     "对话记录不会丢。").arg(code));
    });

    // M5-2: 启动即展示跃动欢迎词（火焰渐变 + 呼吸动画）
    m_breathTimer = new QTimer(this);
    m_breathTimer->setInterval(80); // ~12fps，与命令行版一致
    connect(m_breathTimer, &QTimer::timeout, this, &ChatWidget::updateWelcomeBreath);
    showWelcomeBanner();
}

void ChatWidget::sendMessage()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty() && m_attachments.isEmpty())
        return;
    // 这里不拦「引擎没起来」：拦了会把用户刚打的字丢掉。启动失败由 MainWindow
    // 在开窗时就摆提示；跑到一半崩掉由下面 processExited 的连接负责。
    if (!m_bridge)
        return;
    if (m_welcomeVisible)
        stopWelcomeBreathing(); // M5-2: 发送首条消息后收起欢迎词
    // 用户在生成中直接发新消息 = 用「说话」打断上一轮（这是唯一的键盘打断方式，
    // 产品上不占用 Esc）。本地收流之外必须让引擎把上一轮真取消，否则它还在跑，
    // 迟到的 chunk 会串进新消息里。
    if (m_streaming) {
        endStream();                        // 先收下用户已经看到的那半段
        m_bridge->cancel(m_sessionId);      // 再让引擎别再算上一轮
        m_stopBtn->setVisible(false);
    }

    // M8 附件：引擎 chat.input 目前只收纯字符串（收 [{type:"text"},{type:"image_url"}]
    // 需要改协议），所以把附件绝对路径附在正文后面 —— 引擎侧有读文件工具，
    // 拿到路径就能自己打开。协议升级后再改成结构化 segments。
    QString payload = text;
    if (!m_attachments.isEmpty()) {
        QStringList lines;
        for (const QString &p : m_attachments)
            lines << QStringLiteral("- %1").arg(p);
        if (!payload.isEmpty())
            payload += QStringLiteral("\n\n");
        payload += QStringLiteral("[本地附件]\n") + lines.join(QStringLiteral("\n"));
    }

    appendMessage(QStringLiteral("你"),
                  text.isEmpty() ? QStringLiteral("（发送了附件）") : text);
    if (!m_attachments.isEmpty()) {
        QStringList names;
        for (const QString &p : m_attachments)
            names << QStringLiteral("📎 %1").arg(QFileInfo(p).fileName());
        appendMessage(QStringLiteral("附件"), names.join(QStringLiteral("，")));
    }

    // M8 首条消息 → 交给会话列表当别名，解决「名字看不出会话在干什么」
    if (!m_firstMessageDone && !text.isEmpty()) {
        m_firstMessageDone = true;
        emit firstMessageSent(m_sessionId, text);
    }

    m_input->clear();
    m_attachments.clear();
    m_drafts.remove(m_sessionId); // 已发出，草稿作废
    rebuildAttachChips();
    m_bridge->chat(m_sessionId, payload);
}

// ============================================================
// M8 附件处理：本地文件选择 / 剪贴板图片 / chip 展示与移除
// ============================================================
void ChatWidget::pickFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择要发给 Flare 的本地文件"), QDir::homePath(),
        QStringLiteral("所有文件 (*);;图片 (*.png *.jpg *.jpeg *.webp *.gif *.bmp);;"
                       "文本 (*.txt *.md *.log *.json *.yaml *.yml *.csv)"));
    for (const QString &f : files)
        addAttachment(f);
}

void ChatWidget::addAttachment(const QString &path)
{
    if (path.isEmpty() || !QFileInfo::exists(path))
        return;
    const QString abs = QFileInfo(path).absoluteFilePath();
    if (m_attachments.contains(abs))
        return; // 同一个文件又拖了一次：静默忽略，不必重建整排 chip
    if (m_attachments.size() >= kMaxAttachments) {
        appendMessage(QStringLiteral("⚠️ 附件太多了"),
                      QStringLiteral("一次最多带 %1 个附件，先发这一批再贴剩下的。")
                          .arg(kMaxAttachments));
        return;
    }
    m_attachments << abs;
    rebuildAttachChips();
}

void ChatWidget::removeAttachment(const QString &path)
{
    m_attachments.removeAll(path);
    rebuildAttachChips();
}

void ChatWidget::rebuildAttachChips()
{
    if (!m_attachChips || !m_attachBar)
        return;
    while (QLayoutItem *it = m_attachChips->takeAt(0)) {
        if (QWidget *w = it->widget())
            w->deleteLater();
        delete it;
    }
    for (const QString &p : m_attachments) {
        const QFileInfo fi(p);
        const bool isImage = looksLikeImage(fi);
        auto *chip = new QPushButton(m_attachBar);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setToolTip(p);
        // 图片给 16px 缩略图：只显示文件名时，用户根本认不出自己贴的是哪张图
        // （粘贴进来的文件名是 paste-<毫秒>.png，天然无信息量）
        if (isImage) {
            QImageReader reader(p);
            reader.setAutoTransform(true);
            const QSize orig = reader.size();
            if (orig.isValid() && orig.width() > 0 && orig.height() > 0) {
                // 按目标尺寸解码，不把几十 MP 的原图整张读进内存只为画一个图标
                reader.setScaledSize(orig.scaled(16, 16, Qt::KeepAspectRatioByExpanding));
            }
            const QImage ico = reader.read();
            if (!ico.isNull()) {
                chip->setIcon(QIcon(QPixmap::fromImage(ico)));
                chip->setIconSize(QSize(16, 16));
            }
        }
        // 长文件名会把 chip 撑得横跨整行、把「✕」挤到看不见，统一按像素截断
        const QString name = QFontMetrics(chip->font())
                                 .elidedText(fi.fileName(), Qt::ElideMiddle, 150);
        chip->setText(isImage ? QStringLiteral("%1  ✕").arg(name)
                              : QStringLiteral("📎 %1  ✕").arg(name));
        chip->setStyleSheet(QStringLiteral(
            "QPushButton { background:#fff3e0; color:#b45309; border:1px solid #fcd9a5;"
            " border-radius:12px; padding:4px 10px; font-size:12px; text-align:left; }"
            "QPushButton:hover { background:#ffe3bf; }"));
        const QString path = p;
        connect(chip, &QPushButton::clicked, this, [this, path]() {
            removeAttachment(path);
        });
        m_attachChips->addWidget(chip);
    }
    m_attachChips->addStretch(1);
    m_attachBar->setVisible(!m_attachments.isEmpty());
}

// ============================================================
// M10 拖放：从文件管理器把文件拖进聊天面板 = 挂成附件
// 用户反馈「要有读取本地文件的入口」：📎 是显式入口，拖放是顺手入口，
// 两者共用同一份 m_attachments，走同一条发送链路。
// ============================================================
void ChatWidget::attachLocalFiles(const QList<QUrl> &urls)
{
    int added = 0;
    for (const QUrl &u : urls) {
        if (!u.isLocalFile())
            continue;
        const QString path = u.toLocalFile();
        if (path.isEmpty() || !QFileInfo::exists(path))
            continue;
        addAttachment(path);
        ++added;
    }
    if (added > 0)
        m_input->setFocus(Qt::OtherFocusReason); // 挂上就等用户补一句话
}

void ChatWidget::setDropHighlight(bool on)
{
    if (!m_output)
        return;
    m_output->setStyleSheet(QString::fromLatin1(on ? kOutputStyleDrop : kOutputStyle));
}

void ChatWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData() || !event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    for (const QUrl &u : event->mimeData()->urls()) {
        if (u.isLocalFile()) {
            setDropHighlight(true);
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void ChatWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    setDropHighlight(false);
}

void ChatWidget::dropEvent(QDropEvent *event)
{
    setDropHighlight(false);
    if (!event->mimeData() || !event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    // 文件夹不能当附件：引擎只会把路径拼进 prompt，读不出目录里的内容，
    // 发出去等于给模型一个它打不开的地址。拖文件夹的自然含义是「在这个目录下干活」
    QList<QUrl> files;
    QString dirPath;
    for (const QUrl &u : event->mimeData()->urls()) {
        if (!u.isLocalFile())
            continue;
        if (QFileInfo(u.toLocalFile()).isDir()) {
            if (dirPath.isEmpty())
                dirPath = u.toLocalFile();
            continue;
        }
        files << u;
    }
    if (!files.isEmpty())
        attachLocalFiles(files);
    if (!dirPath.isEmpty())
        emit directoryDropRequested(dirPath);
    event->acceptProposedAction();
}

bool ChatWidget::eventFilter(QObject *watched, QEvent *event)
{
    // 「↓ 新内容」浮标贴在消息区右下角，窗口缩放后要跟着重贴
    if (watched == m_output && event->type() == QEvent::Resize)
        positionJumpHint();
    // M8: 输入框上 Ctrl+V —— 剪贴板是图片就存 png 挂成附件，否则交给 QLineEdit 走文本粘贴
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->matches(QKeySequence::Paste)) {
            const QMimeData *md = QApplication::clipboard()
                                      ? QApplication::clipboard()->mimeData()
                                      : nullptr;
            if (md && md->hasImage()) {
                const QImage img = qvariant_cast<QImage>(md->imageData());
                if (!img.isNull()) {
                    QString dir = QStandardPaths::writableLocation(
                        QStandardPaths::CacheLocation);
                    if (dir.isEmpty())
                        dir = QDir::tempPath() + QStringLiteral("/flare-client");
                    dir += QStringLiteral("/attachments");
                    QDir().mkpath(dir);
                    const QString path = QStringLiteral("%1/paste-%2.png")
                                             .arg(dir)
                                             .arg(QDateTime::currentMSecsSinceEpoch());
                    if (img.save(path, "PNG")) {
                        addAttachment(path);
                        return true; // 已作为附件处理，不再插入文本
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ChatWidget::setSession(const QString &sessionId)
{
    if (sessionId.isEmpty() || sessionId == m_sessionId)
        return;
    // 离开当前会话：把未发出去的字和附件存成该会话的草稿，
    // 空草稿不占位（避免用户切来切去攒一堆空记录）
    if (!m_sessionId.isEmpty()) {
        if (!m_input->text().isEmpty() || !m_attachments.isEmpty())
            m_drafts[m_sessionId] = Draft{m_input->text(), m_attachments};
        else
            m_drafts.remove(m_sessionId);
    }
    // M6-4: 切换会话前若旧会话仍在流式生成，先请求取消，
    // 避免旧会话的 text 事件继续刷到新会话消息区
    if (m_streaming && m_bridge)
        m_bridge->cancel(m_sessionId);
    m_sessionId = sessionId;
    m_streaming = false;
    m_firstMessageDone = false; // M8：新会话重新计首条消息（历史非空时由 renderHistory 置回 true）
    // 恢复目标会话的草稿；附件可能已被用户在文件管理器删掉，落地前滤一遍
    const Draft draft = m_drafts.value(sessionId);
    m_input->setText(draft.text);
    m_attachments = draft.files;
    for (int i = m_attachments.size() - 1; i >= 0; --i) {
        if (!QFileInfo::exists(m_attachments.at(i)))
            m_attachments.removeAt(i);
    }
    rebuildAttachChips();
    m_stopBtn->setVisible(false);
    m_output->clear();
    m_output->setPlaceholderText(QStringLiteral("加载会话历史…"));
    stopWelcomeBreathing();
    // M5-5: 切换会话 → 加载历史消息
    if (m_bridge)
        m_bridge->getMessages(sessionId, 50);
}

// M4-5 易用性：停止当前生成（cancel 协议已存在，UI 暴露）
void ChatWidget::stopGeneration()
{
    if (!m_streaming)
        return;
    endStream();
    m_stopBtn->setVisible(false);
    m_bridge->cancel(m_sessionId);
    appendMessage(QStringLiteral("⏹ 已请求停止"), QStringLiteral("等待引擎取消当前回复…"));
}

void ChatWidget::onEngineEvent(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();

    // M6-4 串会话防护：引擎所有会话事件都带 sessionId；切会话后
    // 旧会话的 text/done/messages 等事件必须忽略，否则串到新会话消息区。
    // （不带 sessionId 的事件视为老协议/测试兼容，放行）
    // error 帧可能不带 sessionId（协议/配置级错误，如未配置 API key）——
    // 必须放行，否则引擎报错时界面一片安静，用户以为消息发出去了。
    if (type != QLatin1String(FlareEvent::Models)
        && type != QLatin1String(FlareEvent::Error)
        && !eventForSession(obj))
        return;

    if (type == QLatin1String(FlareEvent::Messages)) {
        // M5-5: 历史消息加载完成 → 渲染（仅在非流式状态处理，避免覆盖当前对话）
        if (!m_streaming)
            renderHistory(obj.value(QStringLiteral("messages")).toArray());
        return;
    }
    if (type == QLatin1String(FlareEvent::Text)) {
        QString content = obj.value(QStringLiteral("content")).toString();
        if (content.isEmpty())
            content = obj.value(QStringLiteral("text")).toString();
        m_stopBtn->setVisible(true); // M4-5: 流式输出中可停止
        appendStreamChunk(content);
    } else if (type == QLatin1String(FlareEvent::Done)) {
        // 回复流结束：仅关闭流状态，不追加空块
        m_stopBtn->setVisible(false);
        endStream();
    } else if (type == QLatin1String(FlareEvent::Cancelled)) {
        // M6-4: 停止生成已生效（引擎回 cancelled）→ 关闭流 + 收起提示
        m_stopBtn->setVisible(false);
        endStream();
        appendMessage(QStringLiteral("⏹ Flare"), QStringLiteral("已停止生成"));
    } else if (type == QLatin1String(FlareEvent::Error)) {
        m_stopBtn->setVisible(false);
        endStream();
        // 引擎两种 error 帧字段不同：stream 错误用 content，协议错误用 message —— 都要认
        QString msg = obj.value(QStringLiteral("content")).toString();
        if (msg.isEmpty())
            msg = obj.value(QStringLiteral("message")).toString();
        if (msg.isEmpty())
            msg = QStringLiteral("引擎返回了一个没有说明的错误");
        if (msg.contains(QStringLiteral("API key")) || msg.contains(QStringLiteral("密钥"))
            || msg.contains(QStringLiteral("api_key")))
            msg += QStringLiteral("\n\n👉 在左下角「⚙️ 设置」里填入 API Key 后重试。");
        appendMessage(QStringLiteral("⚠️ Flare"), msg);
    } else if (type == QLatin1String(FlareEvent::ToolCall)) {
        m_stopBtn->setVisible(false);
        endStream();
        // M3-4: 卡片式展示工具调用（content 即工具名）
        const QString tool = obj.value(QStringLiteral("content")).toString();
        appendToolCard(QStringLiteral("🔧"), QStringLiteral("调用工具"), tool);
    } else if (type == QLatin1String(FlareEvent::ToolResult)) {
        m_stopBtn->setVisible(false);
        endStream();
        // M3-4: 卡片式展示工具结果（toolName + content）
        const QString toolName = obj.value(QStringLiteral("toolName")).toString();
        QString result = obj.value(QStringLiteral("content")).toString();
        if (result.isEmpty())
            result = obj.value(QStringLiteral("result")).toString();
        appendToolCard(QStringLiteral("📦"), toolName.isEmpty() ? QStringLiteral("工具结果") : toolName, result);
    } else if (type == QLatin1String(FlareEvent::Confirm)) {
        endStream();
        // M3-5: 确认门弹窗（允许/拒绝），按选择回传 confirm_result
        // 【重入安全】QMessageBox::exec() 会启动嵌套事件循环；若在 readyRead
        // 信号处理栈内直接弹窗，新到达的引擎事件会重入本函数 → 崩溃风险。
        // 因此先缓存事件，用 singleShot(0) 延迟到当前事件处理完成后再弹窗。
        m_pendingConfirm = obj;
        if (!m_confirmScheduled) {
            m_confirmScheduled = true;
            QTimer::singleShot(0, this, [this]() {
                m_confirmScheduled = false;
                if (!m_pendingConfirm.isEmpty()) {
                    const QJsonObject evt = m_pendingConfirm;
                    m_pendingConfirm = QJsonObject();
                    showConfirmDialog(evt);
                }
            });
        }
    }
}

// M3-3 流式渲染：首个 text chunk 新建「Flare：」消息块，后续 chunk 增量追加到块尾
void ChatWidget::appendStreamChunk(const QString &chunk)
{
    if (chunk.isEmpty())
        return;
    const bool atBottom = nearBottom();
    QTextCursor cursor(m_output->document());
    cursor.movePosition(QTextCursor::End);
    if (!m_streaming) {
        m_streaming = true;
        cursor.insertBlock();
        QTextCharFormat label;
        label.setForeground(FlameTheme::orange());
        label.setFontWeight(QFont::Bold);
        cursor.insertText(QStringLiteral("Flare："), label);
        m_streamStartPos = cursor.position();   // M9: 正文起点，收流后在这段上做 Markdown 渲染
    }
    cursor.insertText(chunk);
    if (atBottom)
        m_output->setTextCursor(cursor);
    followOrHint(atBottom);
}

void ChatWidget::endStream()
{
    if (!m_streaming) {
        m_streamStartPos = -1;
        return;
    }
    m_streaming = false;

    // M9: 流式期间是纯文本追加（逐 chunk 做 HTML 解析会吃掉正在输入的半个标记），
    // 收流这一刻把这一整段替换成渲染后的富文本：**粗体** / `代码` / 代码块 / 列表都能正常显示。
    const int start = m_streamStartPos;
    m_streamStartPos = -1;
    const int end = m_output->document()->characterCount() - 1;
    if (start < 0 || end <= start)
        return;

    QTextCursor cursor(m_output->document());
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    QString raw = cursor.selectedText();
    raw.replace(QChar(0x2029), QLatin1Char('\n'));   // 段落分隔符还原成换行
    if (raw.trimmed().isEmpty())
        return;
    const bool atBottom = nearBottom();
    const int saved = m_output->verticalScrollBar()->value();
    cursor.insertHtml(MarkdownLite::toHtml(raw));
    if (atBottom) {
        scrollToBottom();
    } else {
        m_output->verticalScrollBar()->setValue(saved);
    }
}

// M3-4: 工具调用/结果卡片 —— 浅紫底 + 圆角边框 + 图标标题 + 等宽内容
void ChatWidget::appendToolCard(const QString &icon, const QString &title, const QString &body)
{
    const bool atBottom = nearBottom();
    QTextCursor cursor(m_output->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    const QString html = QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\"><tr><td "
        "style=\"background:#fff3d6;border:1px solid #fde6bf;border-radius:8px;padding:6px 10px;\">"
        "<b style=\"color:#f97316;\">%1 %2</b>"
        "<br/><span style=\"color:#8a6a3b;font-family:monospace;\">%3</span>"
        "</td></tr></table>")
                             .arg(icon, title.toHtmlEscaped(), body.toHtmlEscaped());
    cursor.insertHtml(html);
    cursor.insertBlock();
    followOrHint(atBottom);
}

// M3-5: 确认门弹窗 —— 浅色紫配 QMessageBox，允许=allow_once / 拒绝=deny
void ChatWidget::showConfirmDialog(const QJsonObject &confirmEvent)
{
    const QString id = confirmEvent.value(QStringLiteral("id")).toString();
    const QString tool = confirmEvent.value(QStringLiteral("name")).toString();
    const QString desc = confirmEvent.value(QStringLiteral("description")).toString();

    // 只展示参数键名（不含值），避免敏感信息暴露
    QStringList argKeys;
    const QJsonObject args = confirmEvent.value(QStringLiteral("args")).toObject();
    for (auto it = args.constBegin(); it != args.constEnd(); ++it)
        argKeys << it.key();

    QString text = QStringLiteral("AI 想调用工具「%1」").arg(tool.isEmpty() ? QStringLiteral("未知") : tool);
    if (!desc.isEmpty())
        text += QStringLiteral("\n说明：%1").arg(desc);
    if (!argKeys.isEmpty())
        text += QStringLiteral("\n参数：%1").arg(argKeys.join(QStringLiteral(", ")));

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("确认操作"));
    box.setText(text);
    box.setStyleSheet(QStringLiteral(
        "QMessageBox { background:#ffffff; }"
        "QLabel { color:#4a2e0d; font-size:14px; }"
        "QPushButton { background:#f97316; color:white; border:none; border-radius:6px;"
        " padding:6px 16px; font-size:14px; }"
        "QPushButton:hover { background:#e0630f; }"));
    QPushButton *allowBtn = box.addButton(QStringLiteral("允许"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("拒绝"), QMessageBox::RejectRole);
    box.setDefaultButton(allowBtn);
    box.exec();

    const QString decision = (box.clickedButton() == allowBtn)
                                 ? QStringLiteral("allow_once")
                                 : QStringLiteral("deny");
    respondConfirm(id, decision);
}

void ChatWidget::respondConfirm(const QString &id, const QString &decision)
{
    if (id.isEmpty())
        return;
    m_bridge->confirmResult(m_sessionId, id, decision);
}

bool ChatWidget::atLatestPosition(int value, int maximum)
{
    if (maximum <= 0)
        return false; // 范围还没算出来，不能据此收提示
    return value >= maximum - 48; // 48px 容差：贴底一点点也算在最新位置
}

bool ChatWidget::nearBottom() const
{
    const QScrollBar *bar = m_output->verticalScrollBar();
    // 48px 容差：贴着底部一点点也算「在看最新内容」
    return bar->value() >= bar->maximum() - 48;
}

void ChatWidget::scrollToBottom()
{
    QScrollBar *bar = m_output->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ChatWidget::followOrHint(bool atBottom)
{
    if (atBottom) {
        scrollToBottom();
        if (m_jumpBtn)
            m_jumpBtn->hide();
        return;
    }
    // 用户在看历史：内容照常进文档，但不抢他的位置，只给一个可点的提示
    if (m_jumpBtn && m_jumpBtn->isHidden()) {
        positionJumpHint();
        m_jumpBtn->show();
        m_jumpBtn->raise();
    }
}

void ChatWidget::positionJumpHint()
{
    if (!m_jumpBtn || !m_output)
        return;
    const int margin = 14;
    m_jumpBtn->adjustSize();
    m_jumpBtn->move(m_output->width() - m_jumpBtn->width() - margin,
                    m_output->height() - m_jumpBtn->height() - margin);
}

void ChatWidget::showEngineNotice(const QString &text)
{
    appendMessage(QStringLiteral("⚠️ 引擎"), text);
}

void ChatWidget::appendMessage(const QString &who, const QString &text)
{
    const bool atBottom = nearBottom();
    const int saved = m_output->verticalScrollBar()->value();
    QString body = text.toHtmlEscaped();
    body.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));

    // 右侧气泡 = 用户说的话；左侧 = Flare 的回答；居中弱化 = 系统提示。
    // 长对话里必须一眼扫出「哪句是我说的」，所以用户的发言右对齐 + 浅琥珀底。
    // 前缀「你：」保留在气泡里，纯文本导出（outputText / 日志 / e2e 断言）仍然可读。
    QString html;
    if (who == QStringLiteral("你")) {
        html = QStringLiteral(
                   "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"7\" border=\"0\">"
                   "<tr><td width=\"18%\"></td>"
                   "<td bgcolor=\"#fff3d6\">"
                   "<span style=\"color:#b45309;font-weight:600;\">你：</span>"
                   "<span style=\"color:#4a2e0d;\">%1</span>"
                   "</td></tr></table>")
                   .arg(body);
    } else if (who == QStringLiteral("Flare")) {
        html = QStringLiteral(R"(<p><b style="color:#f97316;">Flare</b>：%1</p>)").arg(body);
    } else {
        html = QStringLiteral(
                   "<p align=\"center\"><span style=\"color:#8a6a3b;\">%1 %2</span></p>")
                   .arg(who.toHtmlEscaped(), body);
    }

    m_output->append(html);
    if (!atBottom)
        m_output->verticalScrollBar()->setValue(saved); // 守住用户当前阅读位置
    followOrHint(atBottom);
}

// ============================================================
// M5-2 跃动欢迎词（移植命令行版 flame-banner）
// 展示: "F L A R E"（火焰渐变）+ "Let your inspiration flare 🔥"
// 呼吸动画: QTimer 80ms 刷新，相位随 sin 波动（与 CLI 同款）
// ============================================================
void ChatWidget::setSuggestVisible(bool visible)
{
    if (m_suggestBar)
        m_suggestBar->setVisible(visible);
}

bool ChatWidget::suggestVisible() const
{
    return m_suggestBar && !m_suggestBar->isHidden();
}

void ChatWidget::dismissWelcome()
{
    stopWelcomeBreathing();
}

void ChatWidget::showWelcomeBanner()
{
    m_output->clear();
    m_output->setPlaceholderText(QString());
    m_welcomeVisible = true;
    updateWelcomeBreath(); // 立即画第一帧
    m_breathTimer->start();
    setSuggestVisible(true); // 海报下面给出「可以点这个开始」
}

void ChatWidget::startWelcomeBreathing()
{
    if (m_breathTimer && !m_breathTimer->isActive())
        m_breathTimer->start();
}

void ChatWidget::stopWelcomeBreathing()
{
    if (m_breathTimer)
        m_breathTimer->stop();
    m_welcomeVisible = false;
    // 引导只在「还没开始」时出现；一旦开始对话就收起来，别变成常驻噪音
    setSuggestVisible(false);
}

void ChatWidget::updateWelcomeBreath()
{
    if (!m_welcomeVisible)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // 呼吸偏移（与 CLI 一致：5s 周期，振幅 0.12）
    const double t = now / 1000.0;
    const double breath = std::sin(t * M_PI * 2 / 5) * 0.12;

    // F L A R E：每字母独立渐变相位 + 呼吸偏移
    const QString flareText = QStringLiteral("F L A R E");
    const auto chars = flareText.toUcs4();
    const int len = chars.size();
    QString flareHtml;
    for (int i = 0; i < len; ++i) {
        const char32_t cp = static_cast<char32_t>(chars[i]);
        if (cp == 0x20) {
            flareHtml += QLatin1Char(' ');
            continue;
        }
        double phase = (double(i) / len + breath);
        phase -= std::floor(phase);
        const QColor c = FlameTheme::flameColor(phase);
        flareHtml += QStringLiteral("<span style=\"color:%1;font-size:22px;font-weight:700;\">%2</span>")
                         .arg(c.name(),
                              QString::fromUcs4(reinterpret_cast<const char32_t *>(&cp), 1));
    }

    // 标语：句尾落在红色端（reverse）
    const QString tagline = QStringLiteral("Let your inspiration flare");
    const auto tchars = tagline.toUcs4();
    const int tlen = tchars.size();
    QString tagHtml;
    for (int i = 0; i < tlen; ++i) {
        const char32_t cp = static_cast<char32_t>(tchars[i]);
        if (cp == 0x20) {
            tagHtml += QLatin1Char(' ');
            continue;
        }
        double tt = len <= 1 ? 0 : double(i) / (tlen - 1);
        tt = 1 - tt; // reverse：句尾 → 红色端
        const QColor c = FlameTheme::flameColor(tt);
        tagHtml += QStringLiteral("<span style=\"color:%1;\">%2</span>")
                       .arg(c.name(),
                            QString::fromUcs4(reinterpret_cast<const char32_t *>(&cp), 1));
    }

    const QString html = QStringLiteral(
        "<div align=\"center\" style=\"padding-top:40px;\">"
        "<div>%1</div>"
        "<div style=\"padding-top:8px;font-size:15px;\">%2 🔥</div>"
        "<div style=\"padding-top:12px;color:#b45309;font-size:12px;\">开始你的灵感之旅</div>"
        "</div>")
                             .arg(flareHtml, tagHtml);
    m_output->setHtml(html);
    m_output->setAlignment(Qt::AlignHCenter);
}

// M6-4 串会话防护：事件带 sessionId 时校验是否属于当前会话。
// 引擎（flare server）所有会话事件（text/done/cancelled/tool_call/tool_result/
// messages/confirm）都带 sessionId；不带 sessionId 的事件（老协议/测试注入）
// 视为兼容放行 —— 这样测试构造的旧格式事件仍能正常驱动 UI。
bool ChatWidget::eventForSession(const QJsonObject &obj) const
{
    const QJsonValue sid = obj.value(QStringLiteral("sessionId"));
    if (sid.isUndefined() || sid.isNull())
        return true;
    return sid.toString() == m_sessionId;
}

// ============================================================
// M5-5 历史消息加载与渲染
// 切换会话时 get_messages → messages 事件 → 逐条渲染
// ============================================================
void ChatWidget::loadHistory(const QString &sessionId)
{
    if (m_bridge)
        m_bridge->getMessages(sessionId, 50);
}

void ChatWidget::renderHistory(const QJsonArray &messages)
{
    m_output->clear();
    m_output->setPlaceholderText(QString());
    if (messages.isEmpty()) {
        // M6-4: 空会话（新会话/无历史）→ 展示跃动欢迎词（用户要求每个会话都展示）
        showWelcomeBanner();
        return;
    }
    setSuggestVisible(false); // 有历史 = 已经在对话，引导收起来
    m_firstMessageDone = true; // M8：已有历史 → 不再用新消息覆盖会话别名
    for (const QJsonValue &v : messages) {
        const QJsonObject m = v.toObject();
        const QString role = m.value(QStringLiteral("role")).toString();
        const QString content = m.value(QStringLiteral("content")).toString();
        if (content.isEmpty())
            continue;
        if (role == QLatin1String("user")) {
            appendMessage(QStringLiteral("你"), content);
        } else {
            appendMessage(QStringLiteral("Flare"), content);
        }
    }
}
