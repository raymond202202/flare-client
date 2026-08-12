#include "chatwidget.h"

#include <QLabel>
#include <QJsonValue>
#include <QDateTime>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QColor>
#include <QFont>

ChatWidget::ChatWidget(EngineBridge *bridge, QWidget *parent)
    : QWidget(parent)
    , m_bridge(bridge)
    , m_output(new QTextEdit(this))
    , m_input(new QLineEdit(this))
    , m_sendBtn(new QPushButton(QStringLiteral("发送"), this))
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
    layout->addLayout(inputRow);

    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWidget::sendMessage);
    connect(m_input, &QLineEdit::returnPressed, this, &ChatWidget::sendMessage);
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
    m_output->clear();
    m_output->setPlaceholderText(QStringLiteral("已切换到会话 ") + sessionId);
}

void ChatWidget::onEngineEvent(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();

    if (type == QLatin1String(FlareEvent::Text)) {
        QString content = obj.value(QStringLiteral("content")).toString();
        if (content.isEmpty())
            content = obj.value(QStringLiteral("text")).toString();
        appendStreamChunk(content);
    } else if (type == QLatin1String(FlareEvent::Done)) {
        // 回复流结束：仅关闭流状态，不追加空块
        endStream();
    } else if (type == QLatin1String(FlareEvent::Error)) {
        endStream();
        appendMessage(QStringLiteral("⚠️ Flare"), obj.value(QStringLiteral("content")).toString());
    } else if (type == QLatin1String(FlareEvent::ToolCall)) {
        endStream();
        appendMessage(QStringLiteral("🔧 工具"), obj.value(QStringLiteral("content")).toString());
    } else if (type == QLatin1String(FlareEvent::ToolResult)) {
        endStream();
        appendMessage(QStringLiteral("📦 结果"), obj.value(QStringLiteral("content")).toString());
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

void ChatWidget::appendMessage(const QString &who, const QString &text)
{
    const QString html = QStringLiteral(R"(<p><b style="color:#6d4aff;">%1</b>：%2</p>)")
                             .arg(who.toHtmlEscaped(), text.toHtmlEscaped());
    m_output->append(html);
}
