#include "chatwidget.h"

#include <QLabel>
#include <QJsonValue>
#include <QDateTime>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QColor>
#include <QFont>
#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include "flametheme.h"

ChatWidget::ChatWidget(EngineBridge *bridge, QWidget *parent)
    : QWidget(parent)
    , m_bridge(bridge)
    , m_output(new QTextEdit(this))
    , m_input(new QLineEdit(this))
    , m_sendBtn(new QPushButton(QStringLiteral("发送"), this))
    , m_stopBtn(new QPushButton(QStringLiteral("⏹ 停止"), this))
    , m_sessionId(QStringLiteral("gui-") + QString::number(QDateTime::currentMSecsSinceEpoch()))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Flare 对话"), this);
    title->setStyleSheet(QStringLiteral("font-size:16px; font-weight:600; color:#f97316;"));
    layout->addWidget(title);

    // 消息区：只读、可选中复制
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(QStringLiteral("等待对话…"));
    m_output->setStyleSheet(QStringLiteral(
        "QTextEdit { background:#fffbf0; border:1px solid #fde6bf; border-radius:8px;"
        " font-size:14px; padding:8px; }"));
    layout->addWidget(m_output, 1);

    // 输入行
    auto *inputRow = new QHBoxLayout;
    m_input->setPlaceholderText(QStringLiteral("输入消息，Enter 发送"));
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
    connect(m_bridge, &EngineBridge::eventReceived, this, &ChatWidget::onEngineEvent);

    // M5-2: 启动即展示跃动欢迎词（火焰渐变 + 呼吸动画）
    m_breathTimer = new QTimer(this);
    m_breathTimer->setInterval(80); // ~12fps，与命令行版一致
    connect(m_breathTimer, &QTimer::timeout, this, &ChatWidget::updateWelcomeBreath);
    showWelcomeBanner();
}

void ChatWidget::sendMessage()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;
    if (m_welcomeVisible)
        stopWelcomeBreathing(); // M5-2: 发送首条消息后收起欢迎词
    endStream(); // 用户新消息前结束未完成流，避免 chunk 串块
    appendMessage(QStringLiteral("你"), text);
    m_input->clear();
    m_bridge->chat(m_sessionId, text);
}

void ChatWidget::setSession(const QString &sessionId)
{
    if (sessionId.isEmpty() || sessionId == m_sessionId)
        return;
    m_sessionId = sessionId;
    m_streaming = false;
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
    } else if (type == QLatin1String(FlareEvent::Error)) {
        m_stopBtn->setVisible(false);
        endStream();
        appendMessage(QStringLiteral("⚠️ Flare"), obj.value(QStringLiteral("content")).toString());
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
    QTextCursor cursor(m_output->document());
    cursor.movePosition(QTextCursor::End);
    if (!m_streaming) {
        m_streaming = true;
        cursor.insertBlock();
        QTextCharFormat label;
        label.setForeground(FlameTheme::orange());
        label.setFontWeight(QFont::Bold);
        cursor.insertText(QStringLiteral("Flare："), label);
    }
    cursor.insertText(chunk);
    m_output->setTextCursor(cursor);
    m_output->ensureCursorVisible();
}

void ChatWidget::endStream()
{
    m_streaming = false;
}

// M3-4: 工具调用/结果卡片 —— 浅紫底 + 圆角边框 + 图标标题 + 等宽内容
void ChatWidget::appendToolCard(const QString &icon, const QString &title, const QString &body)
{
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
    m_output->setTextCursor(cursor);
    m_output->ensureCursorVisible();
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

void ChatWidget::appendMessage(const QString &who, const QString &text)
{
    const QString html = QStringLiteral(R"(<p><b style="color:#f97316;">%1</b>：%2</p>)")
                             .arg(who.toHtmlEscaped(), text.toHtmlEscaped());
    m_output->append(html);
}

// ============================================================
// M5-2 跃动欢迎词（移植命令行版 flame-banner）
// 展示: "F L A R E"（火焰渐变）+ "Let your inspiration flare 🔥"
// 呼吸动画: QTimer 80ms 刷新，相位随 sin 波动（与 CLI 同款）
// ============================================================
void ChatWidget::showWelcomeBanner()
{
    m_output->clear();
    m_output->setPlaceholderText(QString());
    m_welcomeVisible = true;
    updateWelcomeBreath(); // 立即画第一帧
    m_breathTimer->start();
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
    if (messages.isEmpty())
        m_output->setPlaceholderText(QStringLiteral("该会话暂无消息"));
}
