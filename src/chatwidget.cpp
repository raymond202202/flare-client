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
    title->setStyleSheet(QStringLiteral("font-size:16px; font-weight:600; color:#6d4aff;"));
    layout->addWidget(title);

    // 消息区：只读、可选中复制
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(QStringLiteral("等待对话…"));
    m_output->setStyleSheet(QStringLiteral(
        "QTextEdit { background:#fafafe; border:1px solid #e8e6f5; border-radius:8px;"
        " font-size:14px; padding:8px; }"));
    layout->addWidget(m_output, 1);

    // 输入行
    auto *inputRow = new QHBoxLayout;
    m_input->setPlaceholderText(QStringLiteral("输入消息，Enter 发送"));
    m_input->setStyleSheet(QStringLiteral(
        "QLineEdit { background:#ffffff; border:1px solid #e8e6f5; border-radius:8px;"
        " padding:8px 10px; font-size:14px; }"
        "QLineEdit:focus { border-color:#6d4aff; }"));
    inputRow->addWidget(m_input, 1);
    m_sendBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#6d4aff; color:white; border:none; border-radius:8px;"
        " padding:8px 18px; font-size:14px; }"
        "QPushButton:hover { background:#5a3de0; }"
        "QPushButton:pressed { background:#4b31c0; }"));
    inputRow->addWidget(m_sendBtn);

    // M4-5 易用性：停止生成按钮（默认隐藏，流式输出/等待回复时显示）
    m_stopBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#6d4aff; border:1px solid #d8ccff;"
        " border-radius:8px; padding:8px 14px; font-size:14px; }"
        "QPushButton:hover { background:#f3efff; }"
        "QPushButton:pressed { background:#e8e0ff; }"));
    m_stopBtn->setVisible(false);
    inputRow->addWidget(m_stopBtn);
    layout->addLayout(inputRow);

    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWidget::sendMessage);
    connect(m_input, &QLineEdit::returnPressed, this, &ChatWidget::sendMessage);
    connect(m_stopBtn, &QPushButton::clicked, this, &ChatWidget::stopGeneration);
    connect(m_bridge, &EngineBridge::eventReceived, this, &ChatWidget::onEngineEvent);
}

void ChatWidget::sendMessage()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;
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
    m_output->setPlaceholderText(QStringLiteral("已切换到会话 ") + sessionId);
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
        label.setForeground(QColor(QStringLiteral("#6d4aff")));
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
        "style=\"background:#f3efff;border:1px solid #d8ccff;border-radius:8px;padding:6px 10px;\">"
        "<b style=\"color:#6d4aff;\">%1 %2</b>"
        "<br/><span style=\"color:#4a4a6a;font-family:monospace;\">%3</span>"
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
        "QLabel { color:#2b2b40; font-size:14px; }"
        "QPushButton { background:#6d4aff; color:white; border:none; border-radius:6px;"
        " padding:6px 16px; font-size:14px; }"
        "QPushButton:hover { background:#5a3de0; }"));
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
    const QString html = QStringLiteral(R"(<p><b style="color:#6d4aff;">%1</b>：%2</p>)")
                             .arg(who.toHtmlEscaped(), text.toHtmlEscaped());
    m_output->append(html);
}
