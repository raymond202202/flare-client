#include "sessionlistwidget.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>

// 侧边栏固定宽度（简洁轻量）
static constexpr int kSidebarWidth = 220;

// M5-4: ISO 时间 → 简短显示（今天显示 HH:MM，否则 MM-DD）
static QString shortTime(const QString &iso)
{
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        return iso.left(10);
    if (dt.date() == QDate::currentDate())
        return dt.toString(QStringLiteral("HH:mm"));
    return dt.toString(QStringLiteral("MM-dd"));
}

SessionListWidget::SessionListWidget(EngineBridge *bridge, QWidget *parent)
    : QWidget(parent)
    , m_bridge(bridge)
    , m_list(new QListWidget(this))
    , m_newBtn(new QPushButton(QStringLiteral("＋ 新建"), this))
    , m_delBtn(new QPushButton(QStringLiteral("删除"), this))
    , m_memoryBtn(new QPushButton(QStringLiteral("🧠 记忆"), this))
    , m_skillBtn(new QPushButton(QStringLiteral("🔧 技能"), this))
    , m_settingsBtn(new QPushButton(QStringLiteral("⚙️ 设置"), this))
    , m_searchBox(new QLineEdit(this))
    , m_archiveBtn(new QPushButton(QStringLiteral("📦 归档"), this))
{
    setFixedWidth(kSidebarWidth);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("会话"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; color:#f97316;"));
    layout->addWidget(title);

    // M7-2: 搜索框 —— 输入关键词 → search_sessions 实时过滤
    m_searchBox->setPlaceholderText(QStringLiteral("🔍 搜索会话…"));
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setStyleSheet(QStringLiteral(
        "QLineEdit { background:#ffffff; border:1px solid #fde6bf; border-radius:6px;"
        " padding:6px 8px; font-size:13px; color:#4b5563; }"
        "QLineEdit:focus { border-color:#f97316; }"));
    layout->addWidget(m_searchBox);

    // 会话列表
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background:#fffbf0; border:1px solid #fde6bf; border-radius:8px;"
        " font-size:13px; padding:4px; outline:none; }"
        "QListWidget::item { padding:8px 6px; border-radius:6px; margin:1px; }"
        "QListWidget::item:hover { background:#ffedd0; }"
        "QListWidget::item:selected { background:#f97316; color:white; }"));
    layout->addWidget(m_list, 1);

    // 按钮行
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    m_newBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#f97316; color:white; border:none; border-radius:6px;"
        " padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#e0630f; }"));
    m_delBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#f97316; border:1px solid #fde6bf;"
        " border-radius:6px; padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#ffedd0; }"));
    btnRow->addWidget(m_newBtn, 1);
    btnRow->addWidget(m_delBtn, 1);
    layout->addLayout(btnRow);

    // M7-2: 归档入口 —— archived_sessions 列表
    m_archiveBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#f59e0b; border:1px solid #fde6bf;"
        " border-radius:6px; padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#ffedd0; }"));
    layout->addWidget(m_archiveBtn);

    // 记忆入口（M3-2）
    m_memoryBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#f97316; border:1px solid #fde6bf;"
        " border-radius:6px; padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#ffedd0; }"));
    layout->addWidget(m_memoryBtn);

    // 技能入口（M5-3）—— 用户反馈：有记忆按钮但没 skill 按钮
    m_skillBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#f97316; border:1px solid #fde6bf;"
        " border-radius:6px; padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#ffedd0; }"));
    layout->addWidget(m_skillBtn);

    // 设置入口（M6-2）
    m_settingsBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#ffffff; color:#f97316; border:1px solid #fde6bf;"
        " border-radius:6px; padding:6px 10px; font-size:13px; }"
        "QPushButton:hover { background:#ffedd0; }"));
    layout->addWidget(m_settingsBtn);

    connect(m_bridge, &EngineBridge::eventReceived, this, &SessionListWidget::onEngineEvent);
    connect(m_newBtn, &QPushButton::clicked, this, &SessionListWidget::onCreateClicked);
    connect(m_delBtn, &QPushButton::clicked, this, &SessionListWidget::onDeleteClicked);
    connect(m_memoryBtn, &QPushButton::clicked, this, &SessionListWidget::memoryRequested);
    connect(m_skillBtn, &QPushButton::clicked, this, &SessionListWidget::skillRequested);
    connect(m_settingsBtn, &QPushButton::clicked, this, &SessionListWidget::settingsRequested);
    connect(m_list, &QListWidget::itemClicked, this, &SessionListWidget::onItemClicked);
    // M7-2
    connect(m_searchBox, &QLineEdit::textChanged, this, &SessionListWidget::onSearchChanged);
    connect(m_archiveBtn, &QPushButton::clicked, this, &SessionListWidget::onArchiveClicked);
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
        m_bridge->recentSessions(); // M5-4: 含 preview + updatedAt
}

void SessionListWidget::onEngineEvent(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String(FlareEvent::Sessions)) {
        applySessions(obj.value(QStringLiteral("sessions")).toArray());
    } else if (type == QLatin1String(FlareEvent::RecentSessions)) {
        applySessions(obj.value(QStringLiteral("sessions")).toArray());
    } else if (type == QLatin1String("search_sessions")) {
        // M7-2: 搜索命中 —— 仅当搜索框仍有关键词时应用（防旧响应覆盖新列表）
        if (!m_searchBox->text().trimmed().isEmpty())
            applySessions(obj.value(QStringLiteral("sessions")).toArray());
    } else if (type == QLatin1String("archived_sessions")) {
        // M7-2: 归档列表
        applySessions(obj.value(QStringLiteral("sessions")).toArray());
    } else if (type == QLatin1String(FlareEvent::Ok)) {
        // create/delete 回执 → 重新拉取（标题可能被服务端规整）
        refresh();
    }
}

void SessionListWidget::onSearchChanged(const QString &text)
{
    if (!m_bridge)
        return;
    const QString q = text.trimmed();
    if (q.isEmpty()) {
        // 清空搜索 → 回到最近会话列表
        refresh();
    } else {
        m_bridge->searchSessions(q, 30);
    }
}

void SessionListWidget::onArchiveClicked()
{
    if (!m_bridge)
        return;
    // 切换归档视图：清空搜索框避免干扰（archived_sessions 是独立列表）
    m_searchBox->clear();
    m_bridge->archivedSessions();
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

        // M5-4: 双行展示 —— 标题 + 预览/更新时间，让会话可辨识
        const QString preview = s.value(QStringLiteral("preview")).toString().trimmed();
        const QString updated = s.value(QStringLiteral("updatedAt")).toString();
        QString secondLine;
        if (!preview.isEmpty()) {
            secondLine = preview;
            if (secondLine.size() > 24)
                secondLine = secondLine.left(24) + QStringLiteral("…");
            if (!updated.isEmpty())
                secondLine += QStringLiteral("  ·  ") + shortTime(updated);
        } else if (!updated.isEmpty()) {
            secondLine = shortTime(updated);
        }

        const QString mainText = title.isEmpty() ? QStringLiteral("新会话") : title;
        auto *item = new QListWidgetItem;
        if (secondLine.isEmpty()) {
            item->setText(mainText);
        } else {
            item->setText(QStringLiteral("%1\n%2").arg(mainText, secondLine));
            item->setSizeHint(QSize(180, 46));
            item->setToolTip(preview);
        }
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
