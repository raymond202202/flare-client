#include "chatwidget.h"

#include <QLabel>
#include <QJsonValue>
#include <QDateTime>

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
    appendMessage(QStringLiteral("你"), text);
    m_input->clear();
    m_bridge->chat(m_sessionId, text);
}

void ChatWidget::onEngineEvent(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();

    if (type == QLatin1String(FlareEvent::Text)) {
        appendMessage(QStringLiteral("Flare"), obj.value(QStringLiteral("content")).toString());
    } else if (type == QLatin1String(FlareEvent::Error)) {
        appendMessage(QStringLiteral("⚠️ Flare"), obj.value(QStringLiteral("content")).toString());
    } else if (type == QLatin1String(FlareEvent::ToolCall)) {
        appendMessage(QStringLiteral("🔧 工具"), obj.value(QStringLiteral("content")).toString());
    } else if (type == QLatin1String(FlareEvent::ToolResult)) {
        appendMessage(QStringLiteral("📦 结果"), obj.value(QStringLiteral("content")).toString());
    }
}

void ChatWidget::appendMessage(const QString &who, const QString &text)
{
    const QString html = QStringLiteral("<p><b style=\"color:#6d4aff;\">%1</b>：%2</p>")
                             .arg(who.toHtmlEscaped(), text.toHtmlEscaped());
    m_output->append(html);
}
