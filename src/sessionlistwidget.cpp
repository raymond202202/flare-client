#include "sessionlistwidget.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QKeySequence>
#include <QJsonValue>
#include <QToolTip>
#include <QMessageBox>
#include <QPoint>

// 侧边栏固定宽度（简洁轻量）
static constexpr int kSidebarWidth = 220;
// 侧栏 220px 去掉边框/内距/滚动条后留给文字的实际像素宽度
static constexpr int kItemTextWidth = 168;

// 会话项携带的数据角色
static constexpr int kRoleId = Qt::UserRole;       // 协议 sessionId
static constexpr int kRoleTitle = Qt::UserRole + 1; // 引擎返回的原始 title
static constexpr int kRolePreview = Qt::UserRole + 2;

// 置顶记录落在 ~/.config/flare/flare-client.ini（引擎 pinned 列就绪前的宿主侧方案）
static QSettings pinnedSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("flare"), QStringLiteral("flare-client"));
}

// 引擎建库默认 title='新会话'，全仓没有「首条消息自动生成标题」——这类占位标题
// 用户根本认不出会话是干什么的，宿主侧用首条消息预览顶上（M8）
static bool isPlaceholderTitle(const QString &title)
{
    const QString t = title.trimmed();
    return t.isEmpty()
        || t == QStringLiteral("新会话")
        || t == QStringLiteral("default")
        || t == QStringLiteral("untitled")
        || t.startsWith(QStringLiteral("gui-"));
}

// M5-4: ISO 时间 → 简短显示。M13 之后列表已有「今天/昨天/最近 7 天/更早」
// 分组标题，条目的时间只用来补具体时刻：
//   今天 → HH:mm（标题已说明是哪天）
//   今年 → MM-dd
//   跨年 → yyyy-MM-dd（只写 03-15 谁都分不清是哪一年的三月十五）
QString SessionListWidget::shortStamp(const QString &iso, const QDateTime &now)
{
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        return iso.left(10);
    const QDate today = now.date();
    if (dt.date() == today)
        return dt.toString(QStringLiteral("HH:mm"));
    if (dt.date().year() == today.year())
        return dt.toString(QStringLiteral("MM-dd"));
    return dt.toString(QStringLiteral("yyyy-MM-dd"));
}

static QString shortTime(const QString &iso)
{
    return SessionListWidget::shortStamp(iso, QDateTime::currentDateTime());
}

// M13: 会话按时间分组。列表长了以后一屏全是「周三/周四」的 MM-dd，
// 找「昨天聊的那个」只能一个个点开看。分组标题把扫读成本压到最低。
// 纯函数，便于单测（不依赖当前时间，now 由调用方给）。
static QString sessionSection(const QString &iso, const QDateTime &now)
{
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        return QStringLiteral("更早");
    const QDate today = now.date();
    const QDate d = dt.date();
    if (d == today)
        return QStringLiteral("今天");
    if (d == today.addDays(-1))
        return QStringLiteral("昨天");
    if (d >= today.addDays(-6))
        return QStringLiteral("最近 7 天");
    return QStringLiteral("更早");
}

// 按像素宽度截断。按字符数截断（left(18)）在 220px 侧栏里会让 18 个汉字
// 超过可视宽度，Qt 自动折行把一条会话撑成三四行，列表就不成样子了。
static QString elidePx(const QString &text, const QFont &font, int maxWidthPx)
{
    const QString oneLine = text.simplified();
    if (maxWidthPx <= 0)
        return QString();
    return QFontMetrics(font).elidedText(oneLine, Qt::ElideRight, maxWidthPx);
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
    , m_menu(new QMenu(this))
{
    setFixedWidth(kSidebarWidth);
    // 自动化验收定位锚点：e2e 曾经用 findChild<QLineEdit*>() 盲取第一个控件，
    // 被这里的搜索框抢走，误报「消息没发出去」。每个可交互控件都留名字。
    m_list->setObjectName(QStringLiteral("sessionList"));
    m_searchBox->setObjectName(QStringLiteral("sessionSearch"));
    m_newBtn->setObjectName(QStringLiteral("sessionNewButton"));
    m_delBtn->setObjectName(QStringLiteral("sessionDeleteButton"));
    m_archiveBtn->setObjectName(QStringLiteral("sessionTrashButton"));
    m_memoryBtn->setObjectName(QStringLiteral("sessionMemoryButton"));
    m_skillBtn->setObjectName(QStringLiteral("sessionSkillButton"));
    m_settingsBtn->setObjectName(QStringLiteral("sessionSettingsButton"));
    loadPinned();
    loadAliases();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 标题行：「会话」+ 右侧「🗑️ 回收站」入口
    // M8 打磨：回收站必须有常驻可见入口 —— 只能在会话项右键里打开时，
    // 用户删空列表后就再也进不去了（回收站等于不存在）。
    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(4);
    auto *title = new QLabel(QStringLiteral("会话"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; color:#f97316;"));
    m_archiveBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:transparent; color:#b45309; border:none;"
        " font-size:12px; padding:0 2px; }"
        "QPushButton:hover { color:#f97316; text-decoration:underline; }"));
    m_archiveBtn->setText(QStringLiteral("🗑️ 回收站"));
    m_archiveBtn->setToolTip(QStringLiteral("打开回收站（已删除的会话在这里，可恢复）"));
    m_archiveBtn->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    titleRow->addWidget(m_archiveBtn);
    layout->addLayout(titleRow);

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
    // 硬换行（标题/预览两行）继续生效，但绝不软折行
    m_list->setWordWrap(false);
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list, 1);

    // M8 空状态：「什么都没有」的列表必须告诉用户发生了什么、下一步做什么
    m_emptyHint = new QLabel(m_list);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setStyleSheet(QStringLiteral(
        "QLabel { color:#9ca3af; font-size:12px; background:transparent; }"));
    m_emptyHint->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_emptyHint->hide();

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
    // M8 回收站：删除是软删，回收站里可恢复（彻底删除只在回收站右键菜单里）
    m_delBtn->setToolTip(QStringLiteral("移入回收站（可在「🗑️ 回收站」里恢复）"));
    layout->addLayout(btnRow);

    // M8: 归档 / 置顶 / 重命名 / 恢复 / 清空 / 删除 等操作收进会话项右键菜单；
    // 只有「回收站」在标题行保留常驻入口（否则删空列表后无法进入）。

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
    // M8: 右键菜单 + 行内重命名
    connect(m_list, &QWidget::customContextMenuRequested,
            this, &SessionListWidget::onContextMenuRequested);
    connect(m_list, &QListWidget::itemChanged, this, &SessionListWidget::onItemChanged);
    // 双击会话项 = 重命名（与右键菜单同一入口，避免直接编辑时预览行被当成标题）
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        // 分组标题行不是会话，双击它不应该把「当前会话」拿去改名字
        if (!item || item->data(kRoleId).toString().isEmpty())
            return;
        m_list->setCurrentItem(item);
        renameCurrent();
    });

    rebuildMenu();
}

QString SessionListWidget::currentSessionId() const
{
    const QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return QString();
    return item->data(kRoleId).toString();
}

QListWidgetItem *SessionListWidget::currentItem() const
{
    return m_list->currentItem();
}

// ---------------- 置顶（本地持久化） ----------------
void SessionListWidget::loadPinned()
{
    QSettings s = pinnedSettings();
    m_pinned = s.value(QStringLiteral("sessions/pinned")).toStringList();
}

void SessionListWidget::savePinned()
{
    QSettings s = pinnedSettings();
    s.setValue(QStringLiteral("sessions/pinned"), m_pinned);
    s.sync();
}

// ---------------- 本地别名（会话首条消息片段） ----------------
void SessionListWidget::loadAliases()
{
    QSettings s = pinnedSettings();
    const QVariantMap map = s.value(QStringLiteral("sessions/aliases")).toMap();
    m_aliases.clear();
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        m_aliases.insert(it.key(), it.value().toString());
}

void SessionListWidget::saveAliases()
{
    QSettings s = pinnedSettings();
    QVariantMap map;
    for (auto it = m_aliases.constBegin(); it != m_aliases.constEnd(); ++it)
        map.insert(it.key(), it.value());
    s.setValue(QStringLiteral("sessions/aliases"), map);
    s.sync();
}

void SessionListWidget::setLocalAlias(const QString &sessionId, const QString &text)
{
    const QString clean = text.simplified();
    if (sessionId.isEmpty() || clean.isEmpty())
        return;
    m_aliases.insert(sessionId, clean);
    saveAliases();
    // 别名要「立刻」可见：先用缓存重放当前列表，再拉一次引擎纠偏
    if (!m_archivedView && !m_lastRecent.isEmpty())
        applySessions(m_lastRecent);
    refresh();
}

bool SessionListWidget::isPinned(const QString &sessionId) const
{
    return !sessionId.isEmpty() && m_pinned.contains(sessionId);
}

void SessionListWidget::pinCurrent()
{
    const QString id = currentSessionId();
    if (id.isEmpty())
        return;
    if (m_pinned.contains(id))
        m_pinned.removeAll(id);
    else
        m_pinned.append(id);
    savePinned();
    const QString keep = id;
    refresh();
    // refresh 是异步的，先把当前选中恢复成刚操作的那一项
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(kRoleId).toString() == keep) {
            m_list->setCurrentRow(i);
            break;
        }
    }
    rebuildMenu();
}

// ---------------- 视图切换 ----------------
void SessionListWidget::showArchived()
{
    m_archivedView = true;
    m_searchBox->clear();
    // 先重放缓存，避免引擎回执到达前还显示上一个视图的会话（用户会误以为回收站里是这些）
    if (!m_lastArchived.isEmpty())
        applySessions(m_lastArchived);
    else
        applySessions(QJsonArray());
    if (m_bridge)
        m_bridge->archivedSessions();
    m_archiveBtn->setText(QStringLiteral("← 返回会话"));
    m_archiveBtn->setToolTip(QStringLiteral("返回最近的会话列表"));
    rebuildMenu();
    refresh();
}

void SessionListWidget::showRecent()
{
    m_archivedView = false;
    m_searchBox->clear();
    if (!m_lastRecent.isEmpty())
        applySessions(m_lastRecent);
    else
        applySessions(QJsonArray());
    m_archiveBtn->setText(QStringLiteral("🗑️ 回收站"));
    m_archiveBtn->setToolTip(QStringLiteral("打开回收站（已删除的会话在这里，可恢复）"));
    rebuildMenu();
    refresh();
}


void SessionListWidget::onArchiveClicked()
{
    // M8：同一入口切换 —— 归档视图里再点 = 返回最近会话（否则进去出不来）
    if (m_archivedView)
        showRecent();
    else
        showArchived();
}

void SessionListWidget::refresh()
{
    if (!m_bridge)
        return;
    if (m_archivedView)
        m_bridge->archivedSessions();
    else
        m_bridge->recentSessions(); // M5-4: 含 preview + updatedAt
}

void SessionListWidget::onEngineEvent(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String(FlareEvent::Sessions)) {
        m_lastRecent = obj.value(QStringLiteral("sessions")).toArray();
        applySessions(m_lastRecent);
    } else if (type == QLatin1String(FlareEvent::RecentSessions)) {
        m_lastRecent = obj.value(QStringLiteral("sessions")).toArray();
        applySessions(m_lastRecent);
    } else if (type == QLatin1String("search_sessions")) {
        // M7-2: 搜索命中 —— 仅当搜索框仍有关键词时应用（防旧响应覆盖新列表）
        if (!m_searchBox->text().trimmed().isEmpty())
            applySessions(obj.value(QStringLiteral("sessions")).toArray());
    } else if (type == QLatin1String("archived_sessions")) {
        // M8：只在归档视图内填充，否则会把「最近会话」列表顶成归档内容
        m_lastArchived = obj.value(QStringLiteral("sessions")).toArray();
        if (m_archivedView)
            applySessions(m_lastArchived);
    } else if (type == QLatin1String(FlareEvent::Ok)) {
        // create/delete/rename/end/restore/clear 回执 → 重新拉取
        refresh();
    }
}

void SessionListWidget::onSearchChanged(const QString &text)
{
    if (!m_bridge)
        return;
    const QString q = text.trimmed();
    if (q.isEmpty()) {
        // 清空搜索 → 回到当前视图（最近会话 / 归档箱）
        refresh();
    } else {
        m_bridge->searchSessions(q, 30);
    }
}

void SessionListWidget::applySessions(const QJsonArray &sessions)
{
    const QString prevId = currentSessionId();

    // M8: 置顶项排最前，其余保持引擎返回的时间序
    QList<QJsonObject> pinnedItems;
    QList<QJsonObject> normalItems;
    for (const QJsonValue &v : sessions) {
        const QJsonObject s = v.toObject();
        const QString id = s.value(QStringLiteral("id")).toString();
        if (id.isEmpty())
            continue;
        if (isPinned(id))
            pinnedItems.append(s);
        else
            normalItems.append(s);
    }
    pinnedItems.append(normalItems);

    m_list->clear();

    // 分组标题行：不可选中、不可编辑、不带 sessionId，点它什么也不发生
    auto appendSectionHeader = [this](const QString &label) {
        auto *h = new QListWidgetItem;
        h->setFlags(Qt::NoItemFlags);
        h->setText(QStringLiteral("  ") + label);
        QFont hf = m_list->font();
        hf.setBold(true);
        hf.setPointSizeF(qMax(7.5, hf.pointSizeF() - 1.5));
        // 拉开一点字距：分组标题是「路标」，不是会话名，扫读时该一眼分辨
        hf.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
        h->setFont(hf);
        h->setForeground(QColor(0xa8, 0x96, 0x80));
        h->setSizeHint(QSize(kItemTextWidth, 30));
        h->setTextAlignment(Qt::AlignLeft | Qt::AlignBottom);
        h->setData(kRoleId, QString());   // 明确「没有会话 id」
        m_suppressItemChanged = true;
        m_list->addItem(h);
        m_suppressItemChanged = false;
    };

    const QDateTime now = QDateTime::currentDateTime();
    // 只在「最近会话」主视图分组：搜索结果里一堆没有 updatedAt 的旧会话挤在
    // 「更早」下、回收站里按删除时间分组，都只是噪音。
    const bool groupByDay = m_searchBox->text().trimmed().isEmpty() && !m_archivedView;
    QString currentSection;
    for (const QJsonObject &s : pinnedItems) {
        const QString id = s.value(QStringLiteral("id")).toString();
        const QString title = s.value(QStringLiteral("title")).toString().trimmed();
        const QString preview = s.value(QStringLiteral("preview")).toString().trimmed();
        const QString updated = s.value(QStringLiteral("updatedAt")).toString();
        const QString stamp = updated.isEmpty() ? QString() : shortTime(updated);

        // 空会话不该占列表：引擎建库就写 title='新会话'，既没有真标题、
        // 也没有首条消息预览、也没有本地别名 —— 这种行点开也是空的，纯噪音。
        if (isPlaceholderTitle(title) && preview.isEmpty()
                && m_aliases.value(id).isEmpty())
            continue;

        // M8: 名字要能看出会话在干什么 —— 优先引擎真标题，其次本地别名
        // （该会话首条消息片段），再次引擎给的 preview，最后才是「新会话」占位
        const QString alias = m_aliases.value(id);
        const QFont itemFont = m_list->font();
        const QFontMetrics fm(itemFont);
        const bool pinned = isPinned(id);
        const int pinW = pinned ? fm.horizontalAdvance(QStringLiteral("📌 ")) : 0;
        const int mainW = kItemTextWidth - pinW;
        QString mainText;
        QString secondLine;
        if (!isPlaceholderTitle(title)) {
            mainText = elidePx(title, itemFont, mainW);
            if (!preview.isEmpty()) {
                const QString tail = stamp.isEmpty()
                        ? QString()
                        : QStringLiteral("  ·  ") + stamp;
                secondLine = elidePx(preview, itemFont,
                                     kItemTextWidth - fm.horizontalAdvance(tail)) + tail;
            } else {
                secondLine = elidePx(stamp, itemFont, kItemTextWidth);
            }
        } else if (!alias.isEmpty()) {
            mainText = elidePx(alias, itemFont, mainW);
            secondLine = elidePx(stamp, itemFont, kItemTextWidth);
        } else if (!preview.isEmpty()) {
            mainText = elidePx(preview, itemFont, mainW);
            secondLine = elidePx(stamp, itemFont, kItemTextWidth);
        } else {
            mainText = QStringLiteral("新会话");
            secondLine = elidePx(stamp, itemFont, kItemTextWidth);
        }
        if (pinned)
            mainText.prepend(QStringLiteral("📌 "));

        // 先落分组标题再落会话项：置顶自成一组，其余按今天/昨天/最近 7 天/更早
        const QString section = isPinned(id)
                ? QStringLiteral("📌 置顶")
                : sessionSection(updated, now);
        if (groupByDay && section != currentSection) {
            appendSectionHeader(section);
            currentSection = section;
        }

        auto *item = new QListWidgetItem;
        if (secondLine.isEmpty()) {
            item->setText(mainText);
        } else {
            item->setText(QStringLiteral("%1\n%2").arg(mainText, secondLine));
            item->setSizeHint(QSize(kItemTextWidth, 52));
        }
        item->setToolTip(preview.isEmpty() ? mainText : preview);
        item->setData(kRoleId, id);
        item->setData(kRoleTitle, mainText);
        item->setData(kRolePreview, preview);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_suppressItemChanged = true;
        m_list->addItem(item);
        m_suppressItemChanged = false;
    }

    // 尽量恢复之前的选中项
    if (!prevId.isEmpty()) {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(kRoleId).toString() == prevId) {
                m_list->setCurrentRow(i);
                break;
            }
        }
    }
    updateEmptyHint();
    rebuildMenu();
}

int SessionListWidget::count() const
{
    int n = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        if (!m_list->item(i)->data(kRoleId).toString().isEmpty())
            ++n;
    }
    return n;
}

QListWidgetItem *SessionListWidget::sessionItem(int index) const
{
    int n = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *it = m_list->item(i);
        if (it->data(kRoleId).toString().isEmpty())
            continue;   // 分组标题
        if (n == index)
            return it;
        ++n;
    }
    return nullptr;
}

void SessionListWidget::updateEmptyHint()
{
    if (!m_emptyHint)
        return;
    if (m_list->count() > 0) {
        m_emptyHint->hide();
        return;
    }
    if (!m_searchBox->text().trimmed().isEmpty())
        m_emptyHint->setText(QStringLiteral("没有匹配的会话\n换个关键词试试"));
    else if (m_archivedView)
        m_emptyHint->setText(QStringLiteral("回收站是空的\n在会话上右键即可移入回收站"));
    else
        m_emptyHint->setText(QStringLiteral("还没有会话\n点下方「＋ 新建」开始"));
    m_emptyHint->setGeometry(m_list->rect().adjusted(10, 10, -10, -10));
    m_emptyHint->show();
    m_emptyHint->raise();
}

void SessionListWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_emptyHint && m_emptyHint->isVisible())
        m_emptyHint->setGeometry(m_list->rect().adjusted(10, 10, -10, -10));
}

// ---------------- 右键菜单 ----------------
void SessionListWidget::rebuildMenu()
{
    m_menu->clear();
    const bool hasSel = !currentSessionId().isEmpty();   // 分组标题行没有 id

    // 菜单项也留名字：e2e 要能直接触发「归档」「恢复」而不靠中文文案匹配
    auto *pinAct = m_menu->addAction(hasSel && isPinned(currentSessionId())
                                         ? QStringLiteral("📌 取消置顶")
                                         : QStringLiteral("📌 置顶会话"));
    pinAct->setObjectName(QStringLiteral("actionPin"));
    pinAct->setEnabled(hasSel);
    connect(pinAct, &QAction::triggered, this, &SessionListWidget::pinCurrent);

    auto *renameAct = m_menu->addAction(QStringLiteral("✏️ 重命名会话"));
    renameAct->setObjectName(QStringLiteral("actionRename"));
    renameAct->setEnabled(hasSel);
    renameAct->setShortcut(QKeySequence(Qt::Key_F2));
    connect(renameAct, &QAction::triggered, this, &SessionListWidget::renameCurrent);

    m_menu->addSeparator();

    if (m_archivedView) {
        auto *restoreAct = m_menu->addAction(QStringLiteral("↩️ 恢复到最近会话"));
        restoreAct->setObjectName(QStringLiteral("actionRestore"));
        restoreAct->setEnabled(hasSel);
        connect(restoreAct, &QAction::triggered, this, &SessionListWidget::restoreCurrent);

        auto *purgeAct = m_menu->addAction(QStringLiteral("❌ 彻底删除（不可恢复）"));
        purgeAct->setObjectName(QStringLiteral("actionPurge"));
        purgeAct->setEnabled(hasSel);
        connect(purgeAct, &QAction::triggered, this, &SessionListWidget::purgeCurrent);
    } else {
        auto *archAct = m_menu->addAction(QStringLiteral("📦 归档到归档箱"));
        archAct->setObjectName(QStringLiteral("actionArchive"));
        archAct->setEnabled(hasSel);
        connect(archAct, &QAction::triggered, this, &SessionListWidget::archiveCurrent);
    }

    auto *clearAct = m_menu->addAction(QStringLiteral("🧹 清空会话消息"));
    clearAct->setEnabled(hasSel);
    connect(clearAct, &QAction::triggered, this, &SessionListWidget::clearCurrent);

    if (!m_archivedView) {
        auto *delAct = m_menu->addAction(QStringLiteral("🗑️ 移入回收站"));
        delAct->setObjectName(QStringLiteral("actionTrash"));
        delAct->setEnabled(hasSel);
        connect(delAct, &QAction::triggered, this, &SessionListWidget::deleteCurrent);
    }

    m_menu->addSeparator();
    auto *toggleAct = m_menu->addAction(m_archivedView ? QStringLiteral("📂 返回最近会话")
                                                      : QStringLiteral("🗑️ 打开回收站"));
    toggleAct->setObjectName(QStringLiteral("actionToggleTrash"));
    connect(toggleAct, &QAction::triggered, this, [this]() {
        if (m_archivedView)
            showRecent();
        else
            showArchived();
    });
}

void SessionListWidget::focusSearch()
{
    if (!m_searchBox)
        return;
    m_searchBox->setFocus(Qt::ShortcutFocusReason);
    m_searchBox->selectAll();
}

void SessionListWidget::onContextMenuRequested(const QPoint &pos)
{
    if (QListWidgetItem *item = m_list->itemAt(pos))
        m_list->setCurrentItem(item);
    rebuildMenu();
    m_menu->popup(m_list->viewport()->mapToGlobal(pos));
}

// ---------------- 会话操作 ----------------
void SessionListWidget::renameCurrent()
{
    QListWidgetItem *item = currentItem();
    if (!item || !m_bridge)
        return;
    m_renaming = true;
    m_renamingId = item->data(kRoleId).toString();
    if (m_renamingId.isEmpty())   // 分组标题行
        return;
    m_renamingOriginal = item->data(kRoleTitle).toString();
    // 编辑器里只放标题本体（双行文本进编辑器会把预览也当成标题）
    m_suppressItemChanged = true;
    item->setText(m_renamingOriginal);
    m_suppressItemChanged = false;
    m_list->setCurrentItem(item);
    m_list->editItem(item);
}

void SessionListWidget::onItemChanged(QListWidgetItem *item)
{
    if (m_suppressItemChanged) {
        m_suppressItemChanged = false;
        return;
    }
    if (!m_renaming || !item)
        return;
    m_renaming = false;
    const QString id = m_renamingId;
    const QString original = m_renamingOriginal;
    m_renamingId.clear();
    m_renamingOriginal.clear();

    const QString newTitle = item->text().trimmed();
    if (id.isEmpty() || newTitle.isEmpty() || newTitle == original) {
        refresh(); // 取消 / 空标题 / 未改 → 还原双行显示
        return;
    }
    item->setData(kRoleTitle, newTitle); // 先本地更新，避免回执到达前闪回旧名
    m_bridge->renameSession(id, newTitle);
    emit sessionRenamed(id, newTitle);
}

void SessionListWidget::archiveCurrent()
{
    const QString id = currentSessionId();
    if (id.isEmpty() || !m_bridge)
        return;
    m_bridge->endSession(id);
}

void SessionListWidget::restoreCurrent()
{
    const QString id = currentSessionId();
    if (id.isEmpty() || !m_bridge)
        return;
    m_bridge->restoreSession(id);
}

void SessionListWidget::clearCurrent()
{
    const QString id = currentSessionId();
    if (id.isEmpty() || !m_bridge)
        return;
    m_bridge->clearSession(id);
}

void SessionListWidget::deleteCurrent()
{
    // M8 回收站：这里的「删除」是软删（engine end_session = archive），可在回收站里恢复
    const QString id = currentSessionId();
    if (id.isEmpty() || !m_bridge)
        return;
    if (m_archivedView) {
        purgeCurrent(); // 回收站里再删 = 彻底删除
        return;
    }
    if (isPinned(id)) {
        m_pinned.removeAll(id);
        savePinned();
    }
    m_bridge->endSession(id);
    QToolTip::showText(m_list->viewport()->mapToGlobal(QPoint(8, 8)),
                       QStringLiteral("已移入回收站，可在「🗑️ 回收站」里恢复"),
                       m_list);
}

void SessionListWidget::purgeCurrent()
{
    const QString id = currentSessionId();
    if (id.isEmpty() || !m_bridge)
        return;
    const auto ret = QMessageBox::question(
        this, QStringLiteral("彻底删除会话"),
        QStringLiteral("彻底删除后无法恢复，确定要删除这个会话吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;
    if (isPinned(id)) {
        m_pinned.removeAll(id);
        savePinned();
    }
    m_bridge->deleteSession(id);
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
    // M8 回收站：按钮语义 = 移入回收站（软删，可恢复）；彻底删除只在回收站内确认后发生
    deleteCurrent();
}

void SessionListWidget::onItemClicked(QListWidgetItem *item)
{
    if (!item)
        return;
    rebuildMenu();
    const QString picked = item->data(kRoleId).toString();
    if (picked.isEmpty())   // 分组标题行
        return;
    emit sessionSelected(picked);
}
