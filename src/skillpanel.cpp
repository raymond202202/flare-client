#include "skillpanel.h"
#include "flametheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>

// ============================================================
// 技能/工具面板（M5-3）
// 发 tools 命令 → tools 事件 → 展示可用工具列表（名称+描述）
// ============================================================
SkillPanel::SkillPanel(EngineBridge *bridge, QWidget *parent)
    : QWidget(parent)
    , m_bridge(bridge)
    , m_list(new QListWidget(this))
    , m_refreshBtn(new QPushButton(QStringLiteral("刷新"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("技能与工具"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; color:#f97316;"));
    layout->addWidget(title);

    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background:#fffbf0; border:1px solid #fde6bf; border-radius:8px;"
        " font-size:13px; padding:4px; outline:none; }"
        "QListWidget::item { padding:8px 6px; border-radius:6px; margin:1px; }"
        "QListWidget::item:hover { background:#ffedd0; }"
        "QListWidget::item:selected { background:#f97316; color:white; }"));
    layout->addWidget(m_list, 1);

    m_refreshBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#f97316; color:white; border:none; border-radius:6px;"
        " padding:6px 12px; font-size:13px; }"
        "QPushButton:hover { background:#e0630f; }"));
    layout->addWidget(m_refreshBtn, 0, Qt::AlignRight);

    connect(m_refreshBtn, &QPushButton::clicked, this, &SkillPanel::refresh);
    connect(m_bridge, &EngineBridge::eventReceived, this, &SkillPanel::onEngineEvent);
}

void SkillPanel::refresh()
{
    m_list->clear();
    m_list->addItem(QStringLiteral("加载中…"));
    m_bridge->queryTools();
}

void SkillPanel::onEngineEvent(const QJsonObject &obj)
{
    if (eventType(obj) != QLatin1String(FlareEvent::Tools))
        return;
    applyTools(obj.value(QStringLiteral("tools")).toArray());
}

void SkillPanel::applyTools(const QJsonArray &tools)
{
    m_list->clear();
    for (const auto &v : tools) {
        const QJsonObject tool = v.toObject();
        const QString name = tool.value(QStringLiteral("name")).toString();
        const QString desc = tool.value(QStringLiteral("description")).toString();
        if (name.isEmpty())
            continue;
        const QString display = desc.isEmpty()
                                    ? name
                                    : QStringLiteral("%1 — %2").arg(name, desc);
        auto *item = new QListWidgetItem(display, m_list);
        item->setToolTip(desc);
        item->setData(Qt::UserRole, name);
        m_list->addItem(item);
    }
    if (m_list->count() == 0)
        m_list->addItem(QStringLiteral("（没有可用工具）"));
}
