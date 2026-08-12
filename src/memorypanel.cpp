#include "memorypanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

MemoryPanel::MemoryPanel(EngineBridge *bridge, QWidget *parent)
    : QWidget(parent)
    , m_bridge(bridge)
    , m_list(new QListWidget(this))
    , m_refreshBtn(new QPushButton(QStringLiteral("🔄 刷新"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("记忆"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; color:#6d4aff;"));
    layout->addWidget(title);

    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background:#fafafe; border:1px solid #e8e6f5; border-radius:8px;"
        " font-size:13px; padding:4px; outline:none; }"
        "QListWidget::item { padding:8px 6px; border-radius:6px; margin:1px; }"
        "QListWidget::item:hover { background:#f0edff; }"
        "QListWidget::item:selected { background:#6d4aff; color:white; }"));
    layout->addWidget(m_list, 1);

    m_refreshBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#6d4aff; color:white; border:none; border-radius:6px;"
        " padding:6px 12px; font-size:13px; }"
        "QPushButton:hover { background:#5a3de0; }"));
    layout->addWidget(m_refreshBtn);

    connect(m_bridge, &EngineBridge::eventReceived, this, &MemoryPanel::onEngineEvent);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MemoryPanel::refresh);
}

void MemoryPanel::refresh()
{
    if (m_bridge)
        m_bridge->getMemories();
}

void MemoryPanel::onEngineEvent(const QJsonObject &obj)
{
    if (obj.value(QStringLiteral("type")).toString() == QLatin1String(FlareEvent::Memories))
        applyMemories(obj.value(QStringLiteral("memories")).toArray());
}

void MemoryPanel::applyMemories(const QJsonArray &memories)
{
    m_list->clear();
    for (const QJsonValue &v : memories) {
        const QJsonObject m = v.toObject();
        const QString content = m.value(QStringLiteral("content")).toString().trimmed();
        if (content.isEmpty())
            continue;
        const QString type = m.value(QStringLiteral("type")).toString();
        const QString created = m.value(QStringLiteral("created_at")).toString();
        QString label = content;
        if (!type.isEmpty() && type != QLatin1String("note"))
            label.prepend(QStringLiteral("[%1] ").arg(type));
        if (!created.isEmpty())
            label += QStringLiteral("\n      ") + created;
        auto *item = new QListWidgetItem(label);
        item->setToolTip(content);
        m_list->addItem(item);
    }
}
