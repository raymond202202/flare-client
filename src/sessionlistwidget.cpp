#include "sessionlistwidget.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>

// 侧边栏固定宽度（简洁轻量）
static constexpr int kSidebarWidth = 220;

SessionListWidget::SessionListWidget(EngineBridge *bridge, QWidget *parent)
    : QWidget(parent)
    , m_bridge(bridge)
    , m_list(new QListWidget(this))
    , m_newBtn(new QPushButton(QStringLiteral("＋ 新建"), this))
    , m_delBtn(new QPushButton(QStringLiteral("删除"), this))
    , m_memoryBtn(new QPushButton(QStringLiteral("🧠 记忆"), this))
{
    setFixedWidth(kSidebarWidth);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("会话"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; color:#6d4aff;"));
    layout->addWidget(title);

    // 会话列表
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background:#fafafe; border:1px solid #e8e6f5; border-radius:8px;"
        " font-size:13px; padding:4px; outline:none; }"
        "QListWidget::item { padding:8px 6px; border-radius:6px; margin:1px; }"
        "QListWidget::item:hover { background:#f0edff; }"
        "QListWidget::item:selected { background:#6d4aff; color:white; }"));
    layout->addWidget(m_list, 1);

    // 按钮行
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    m_newBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#6d4aff; color:white; border:none; border-radius:6px;"
        " padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#5a3de0; }"));
    m_delBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#6d4aff; border:1px solid #d9d2f7;"
        " border-radius:6px; padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#f4f2ff; }"));
    btnRow->addWidget(m_newBtn, 1);
    btnRow->addWidget(m_delBtn, 1);
    layout->addLayout(btnRow);

    // 记忆入口（M3-2）
    m_memoryBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#6d4aff; border:1px solid #d9d2f7;"
        " border-radius:6px; padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#f4f2ff; }"));
    layout->addWidget(m_memoryBtn);

    connect(m_bridge, &EngineBridge::eventReceived, this, &SessionListWidget::onEngineEvent);
    connect(m_newBtn, &QPushButton::clicked, this, &SessionListWidget::onCreateClicked);
    connect(m_delBtn, &QPushButton::clicked, this, &SessionListWidget::onDeleteClicked);
    connect(m_memoryBtn, &QPushButton::clicked, this, &SessionListWidget::memoryRequested);
    connect(m_list, &QListWidget::itemClicked, this, &SessionListWidget::onItemClicked);
}

QString SessionListWidget::currentSessionId() const
{
    const QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return QString();
    return item->data(Qt::UserRole).toString();
}

void SessionListWidget::refresh()
{
    if (m_bridge)
        m_bridge->listSessions();
}

void SessionListWidget::onEngineEvent(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String(FlareEvent::Sessions)) {
        applySessions(obj.value(QStringLiteral("sessions")).toArray());
    } else if (type == QLatin1String(FlareEvent::Ok)) {
        // create/delete 回执 → 重新拉取（标题可能被服务端规整）
        refresh();
    }
}

void SessionListWidget::applySessions(const QJsonArray &sessions)
{
    const QString prevId = currentSessionId();
    m_list->clear();
    for (const QJsonValue &v : sessions) {
        const QJsonObject s = v.toObject();
        const QString id = s.value(QStringLiteral("id")).toString();
        const QString title = s.value(QStringLiteral("title")).toString();
        if (id.isEmpty())
            continue;
        auto *item = new QListWidgetItem(title.isEmpty() ? QStringLiteral("新会话") : title);
        item->setData(Qt::UserRole, id);
        m_list->addItem(item);
    }
    // 尽量恢复之前的选中项
    if (!prevId.isEmpty()) {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(Qt::UserRole).toString() == prevId) {
                m_list->setCurrentRow(i);
                break;
            }
        }
    }
}

void SessionListWidget::onCreateClicked()
{
    const QString id = QStringLiteral("gui-") + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (m_bridge)
        m_bridge->createSession(id, QStringLiteral("新会话"));
    // 立即选中并告知主窗口（服务端 ok 后 refresh 会保留选中）
    emit sessionSelected(id);
}

void SessionListWidget::onDeleteClicked()
{
    const QString id = currentSessionId();
    if (id.isEmpty() || !m_bridge)
        return;
    m_bridge->deleteSession(id);
}

void SessionListWidget::onItemClicked(QListWidgetItem *item)
{
    if (!item)
        return;
    emit sessionSelected(item->data(Qt::UserRole).toString());
}
