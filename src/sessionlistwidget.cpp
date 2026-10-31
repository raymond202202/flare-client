#include "sessionlistwidget.h"
#include "flaretime.h"
#include "mediatags.h"

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
#include <QKeyEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include "searchhighlightdelegate.h"
#include <QTimer>
#include <QElapsedTimer>
#include <cstdio>

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

// 会话预览的显示文案：引擎只给事实（preview 纯文本 + previewKinds 说明里面还有
// 图片/附件），「[图片]」「[附件]」这类词由宿主拼 —— 它们属于展示层，引擎一旦写死
// 中文文案，换语言/换宿主就得回来改引擎（修错层③）。
static QString previewLabel(const QJsonObject &s)
{
    return MediaTags::withText(s.value(QStringLiteral("preview")).toString(),
                               s.value(QStringLiteral("previewKinds")).toArray());
}

// M5-4: ISO 时间 → 简短显示。M13 之后列表已有「今天/昨天/最近 7 天/更早」
// 分组标题，条目的时间只用来补具体时刻：
//   今天 → HH:mm（标题已说明是哪天）
//   今年 → MM-dd
//   跨年 → yyyy-MM-dd（只写 03-15 谁都分不清是哪一年的三月十五）
// M15-3：解析与格式统一交给 FlareTime —— 引擎给的是 RFC3339 UTC，老数据可能
// 是 sqlite 的裸串（无时区标记但本身就是 UTC）。以前这里直接 Qt::ISODateWithMs
// 解析，裸串被当成本地时间，刚聊完的会话显示成 8 小时前。
// 标题与预览是不是同一个事实？引擎对占位标题（「新会话」）会用首条用户消息顶替，
// 于是 GUI 拿到的 title 与 preview 内容相同，或 title 恰是 preview 的定长截断。
// 判据只做「互为首缀」，不猜语义。
static bool sameFactText(const QString &title, const QString &preview)
{
    const QString a = title.simplified();
    const QString b = preview.simplified();
    if (a.isEmpty() || b.isEmpty())
        return false;
    return a == b || b.startsWith(a) || a.startsWith(b);
}

QString SessionListWidget::shortStamp(const QString &iso, const QDateTime &now)
{
    return FlareTime::listStamp(iso, now);
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
    // M15-3：同上，按本地时区判定归属哪天，避免凌晨的会话被分进「昨天」
    return FlareTime::section(iso, now);
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

// 搜索激活时按关键词位置截断：普通 ElideRight 从右边切，关键词多半落在
// 窗口靠后段，一切就切没了 —— 副行反而看不出为什么命中。
// 这里改成「让关键词一定在可见区内」，左右该省的部分用省略号表示。
static QString elideKeepingKeyword(const QString &text, const QFont &font, int maxWidthPx,
                                   const QString &keyword)
{
    const QString oneLine = text.simplified();
    if (maxWidthPx <= 0)
        return QString();
    const QFontMetrics fm(font);
    if (keyword.isEmpty() || fm.horizontalAdvance(oneLine) <= maxWidthPx)
        return fm.elidedText(oneLine, Qt::ElideRight, maxWidthPx);

    const int hit = oneLine.indexOf(keyword, 0, Qt::CaseInsensitive);
    if (hit < 0)
        return fm.elidedText(oneLine, Qt::ElideRight, maxWidthPx);

    // 关键词本身比整行还宽（用户粘了一大段）：无解，退回普通截断
    const QString hitText = oneLine.mid(hit, keyword.size());
    if (fm.horizontalAdvance(hitText) >= maxWidthPx)
        return fm.elidedText(oneLine, Qt::ElideRight, maxWidthPx);

    const int room = maxWidthPx - fm.horizontalAdvance(QStringLiteral("…")) * 2
            - fm.horizontalAdvance(hitText);
    // 关键词前面能留多少字：给三分之一，够看清是「哪一句」
    int headRoom = room / 3;
    QString head = oneLine.left(hit);
    while (!head.isEmpty() && fm.horizontalAdvance(head) > headRoom)
        head.remove(0, 1);
    QString tail = oneLine.mid(hit + keyword.size());
    const int tailRoom = room - (head.isEmpty() ? 0 : fm.horizontalAdvance(head));
    while (!tail.isEmpty() && fm.horizontalAdvance(tail) > tailRoom)
        tail.chop(1);
    return (head.isEmpty() ? QString() : QStringLiteral("…") + head) + hitText
            + tail + (tail.length() < oneLine.length() - hit - keyword.size()
                       ? QStringLiteral("…") : QString());
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
    // M14: 键盘 ↑/↓ 也要真的切会话 —— 只连 itemClicked 的话，方向键只动高亮不动内容，
    // 看起来像「列表坏了」。currentItemChanged 覆盖键盘、鼠标、程序化三种来源。
    connect(m_list, &QListWidget::currentItemChanged,
            this, &SessionListWidget::onCurrentItemChanged);
    // 搜索框里敲完字按 ↓ 直接进结果列表，不用再摸鼠标
    m_searchBox->installEventFilter(this);
    // M7-2
    connect(m_searchBox, &QLineEdit::textChanged, this, &SessionListWidget::onSearchChanged);
    // 250ms 防抖：输入停顿后才发一次搜索，避免逐字全表扫
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(250);
    connect(m_searchDebounce, &QTimer::timeout, this, &SessionListWidget::flushSearch);
    connect(m_archiveBtn, &QPushButton::clicked, this, &SessionListWidget::onArchiveClicked);
    // M8: 右键菜单 + 行内重命名
    connect(m_list, &QWidget::customContextMenuRequested,
            this, &SessionListWidget::onContextMenuRequested);
    connect(m_list, &QListWidget::itemChanged, this, &SessionListWidget::onItemChanged);
    // 滚到底自动加载更多：引擎一次只回 m_recentLimit 条，
    // 用户滚到接近底部就把上限抬高 40 条再拉一次（老会话不至于只能靠搜索）
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
        if (!m_bridge || m_archivedView || !m_hasMoreRecent || m_fetchingMore)
            return;
        if (!m_searchBox->text().trimmed().isEmpty())
            return;  // 搜索结果是一个固定集合，不翻页
        QScrollBar *sb = m_list->verticalScrollBar();
        if (sb->maximum() <= 0 || v < sb->maximum() - 4)
            return;  // 没滚到底
        m_fetchingMore = true;
        m_recentLimit += 40;
        m_bridge->recentSessions(m_recentLimit);
    });
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
// 视图切换的唯一搜索状态收敛点：停防抖、清关键词与在途标记、清搜索框。
// 清搜索框必须屏蔽 textChanged —— 否则 clear() 会顺手触发一次 refresh()，
// 叠上调用方自己的 refresh() 就是同一视图连发两次列表请求（列表重建两次）。
void SessionListWidget::resetSearchState()
{
    if (m_searchDebounce)
        m_searchDebounce->stop();
    m_pendingQuery.clear();
    m_inflightQuery.clear();
    if (m_searchBox && !m_searchBox->text().isEmpty()) {
        const QSignalBlocker blocker(m_searchBox);
        m_searchBox->clear();
    }
}

void SessionListWidget::showArchived()
{
    m_archivedView = true;
    resetSearchState();
    // 先重放缓存，避免引擎回执到达前还显示上一个视图的会话（用户会误以为回收站里是这些）
    if (!m_lastArchived.isEmpty())
        applySessions(m_lastArchived);
    else
        applySessions(QJsonArray());
    m_archiveBtn->setText(QStringLiteral("← 返回会话"));
    m_archiveBtn->setToolTip(QStringLiteral("返回最近的会话列表"));
    rebuildMenu();
    refresh();
}

void SessionListWidget::showRecent()
{
    m_archivedView = false;
    // 回到最近会话视图就从头开始翻页
    m_recentLimit = 20;
    m_hasMoreRecent = true;
    m_fetchingMore = false;
    resetSearchState();
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
    if (m_archivedView) {
        m_bridge->archivedSessions();
    } else {
        // M5-4: 含 preview + updatedAt；带上当前分页上限，
        // 免得刷新一次就把已经翻出来的老会话又收回 20 条
        m_fetchingMore = false;
        m_bridge->recentSessions(m_recentLimit);
    }
}

void SessionListWidget::onEngineEvent(const QJsonObject &obj)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String(FlareEvent::Sessions)) {
        m_lastRecent = obj.value(QStringLiteral("sessions")).toArray();
        applySessions(m_lastRecent);
    } else if (type == QLatin1String(FlareEvent::RecentSessions)) {
        m_lastRecent = obj.value(QStringLiteral("sessions")).toArray();
        // 回来的条数不足请求上限 = 已经翻到底，别再触发加载更多
        m_hasMoreRecent = m_lastRecent.size() >= m_recentLimit;
        m_fetchingMore = false;
        applySessions(m_lastRecent);
    } else if (type == QLatin1String("search_sessions")) {
        // M7-2: 搜索命中。
        // 引擎在回执里回显 query，这里必须校对：快速改关键词时，先发的那次
        // 查询可能后到，若直接应用就会把列表顶成上一个关键词的结果，而搜索框
        // 写着新词 —— 用户看到的是「搜的不是我打的字」。
        const QString echoed = obj.value(QStringLiteral("query")).toString().trimmed();
        const QString current = m_searchBox->text().trimmed();
        if (current.isEmpty())
            return;
        if (!echoed.isEmpty() && echoed != current) {
            // 迟到响应：丢弃，等当前关键词自己的回执
            if (qEnvironmentVariableIsSet("FLARE_DEBUG_LIST")) {
                std::fprintf(stderr, "SEARCH_DROP stale=%s current=%s\n",
                             echoed.toUtf8().constData(), current.toUtf8().constData());
                std::fflush(stderr);
            }
            return;
        }
        // 关键词相同也可能是跨视图的旧回执：在回收站里搜过「灵感」，切回主列表
        // 再搜「灵感」，先发的归档查询回来时关键词一致，光比 query 挡不住。
        // 引擎回执里带 archived 范围，跟当前视图不一致就丢。
        const bool echoedArchived = obj.value(QStringLiteral("archived")).toBool(false);
        if (echoedArchived != m_archivedView) {
            if (qEnvironmentVariableIsSet("FLARE_DEBUG_LIST")) {
                std::fprintf(stderr, "SEARCH_DROP view echoed_archived=%d current_archived=%d\n",
                             int(echoedArchived), int(m_archivedView));
                std::fflush(stderr);
            }
            return;
        }
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
        // 清空搜索 → 回到当前视图（最近会话 / 归档箱），立刻响应，不等防抖
        m_pendingQuery.clear();
        m_inflightQuery.clear();
        if (m_searchDebounce)
            m_searchDebounce->stop();
        refresh();
        return;
    }
    // 攒一下再发：中文输入法拼音过程、连续退格都不该每帧打一次全表 LIKE
    m_pendingQuery = q;
    if (!m_searchDebounce) {
        flushSearch();
        return;
    }
    m_searchDebounce->start();
}

void SessionListWidget::flushSearch()
{
    if (!m_bridge || m_pendingQuery.isEmpty())
        return;
    m_inflightQuery = m_pendingQuery;
    ++m_searchSentCount;
    // 搜索范围跟着视图走：回收站里搜的是归档集合（引擎负责分集合，宿主不拼条件）
    m_bridge->searchSessions(m_inflightQuery, 30, m_archivedView);
}

void SessionListWidget::applySessions(const QJsonArray &sessions)
{
    QElapsedTimer applyTimer;
    applyTimer.start();
    const QString prevId = currentSessionId();
    // 分页加载会把列表整体重建，必须先记下滚动位置再还原，
    // 否则用户滚到底触发加载更多时列表会弹回顶部
    const int prevScroll = m_list->verticalScrollBar()->value();
    // 搜索激活时（有 inflight 关键词）副行要保证关键词可见，否则用户看不出
    // 这条为什么命中。非搜索视图为空串，截断行为与以前完全一致。
    const QString highlightKeyword = m_inflightQuery.trimmed();

    // M8: 置顶项排最前，其余保持引擎返回的时间序
    QList<QJsonObject> pinnedItems;
    QList<QJsonObject> normalItems;
    int skippedEmptyId = 0;
    int skippedPlaceholder = 0;
    QString firstSkippedSample;
    for (const QJsonValue &v : sessions) {
        const QJsonObject s = v.toObject();
        const QString id = s.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            ++skippedEmptyId;
            continue;
        }
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
        const QString preview = previewLabel(s);
        const QString updated = s.value(QStringLiteral("updatedAt")).toString();
        const QString stamp = updated.isEmpty() ? QString() : shortTime(updated);

        // 空会话不该占列表：引擎建库就写 title='新会话'，既没有真标题、
        // 也没有首条消息预览 —— 这种行点开也是空的，纯噪音。
        if (isPlaceholderTitle(title) && preview.isEmpty()) {
            // 引擎明明给了命中行，这里却全被跳过时，列表会「有结果却空屏」——
            // 没有这条埋点就只能靠猜。留下首个样本，一眼看出缺的是哪个字段。
            ++skippedPlaceholder;
            if (firstSkippedSample.isEmpty())
                firstSkippedSample = QStringLiteral("%1|title=%2|preview=%3")
                        .arg(id, title, QStringLiteral("%1").arg(preview.size()));
            continue;
        }

        // M8: 名字要能看出会话在干什么 —— 唯一来源是引擎：有真标题用标题，
        // 占位标题（用户还没重命名）时用引擎给的首条消息 preview，最后才是「新会话」。
        // 宿主不再自存一份别名：那份只在本机 ini 里，换机/清配置就丢，且会和引擎的
        // preview 打架（同一个事实两个来源）。
        const QFont itemFont = m_list->font();
        const QFontMetrics fm(itemFont);
        const auto clip = [&](const QString &text, int maxW) {
            return highlightKeyword.isEmpty()
                    ? elidePx(text, itemFont, maxW)
                    : elideKeepingKeyword(text, itemFont, maxW, highlightKeyword);
        };
        const bool pinned = isPinned(id);
        const int pinW = pinned ? fm.horizontalAdvance(QStringLiteral("📌 ")) : 0;
        const int mainW = kItemTextWidth - pinW;
        QString mainText;
        QString secondLine;
        if (!isPlaceholderTitle(title)) {
            mainText = clip(title, mainW);
            // 引擎把占位标题回退成首条消息之后，标题与预览常常是同一句话。
            // 副行再复述一遍就是噪音，这时副行只留时间（全文仍在 tooltip 里）。
            if (!preview.isEmpty() && !sameFactText(title, preview)) {
                const QString tail = stamp.isEmpty()
                        ? QString()
                        : QStringLiteral("  ·  ") + stamp;
                secondLine = clip(preview,
                                  kItemTextWidth - fm.horizontalAdvance(tail)) + tail;
            } else {
                secondLine = elidePx(stamp, itemFont, kItemTextWidth);
            }
        } else if (!preview.isEmpty()) {
            mainText = clip(preview, mainW);
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
    // 搜索高亮：命中词在副行铺琥珀底色。关键词为空就卸掉 delegate，
    // 平常列表的绘制路径跟以前一模一样（不做无谓自绘开销）。
    if (highlightKeyword.isEmpty()) {
        if (m_searchDelegate) {
            m_list->setItemDelegate(nullptr);
            m_searchDelegate->deleteLater();
            m_searchDelegate = nullptr;
            m_searchDelegateKeyword.clear();
        }
    } else if (!m_searchDelegate || m_searchDelegateKeyword != highlightKeyword) {
        m_searchDelegate = new SearchHighlightDelegate(highlightKeyword, this);
        m_list->setItemDelegate(m_searchDelegate);
        m_searchDelegateKeyword = highlightKeyword;
    }
    // 还原滚动位置（重建列表会把滚动条打回顶部）
    if (prevScroll > 0)
        m_list->verticalScrollBar()->setValue(prevScroll);

    // 取证：整表重建到底有多贵（850 条会话时一次重建是真实卡顿来源）。
    // 只在 FLARE_DEBUG_LIST=1 时输出，正常使用不刷屏。
    if (qEnvironmentVariableIsSet("FLARE_DEBUG_LIST")) {
        // 用 fprintf 而不是 qWarning：--screenshot 的其它诊断（LIST_ITEMS/ITEM[i]）
        // 全都走 stderr 直写，取证时一个通道就够，不用挑 Qt 日志规则。
        std::fprintf(stderr, "APPLY ms=%lld rows=%d items=%d scroll=%d skip_noid=%d skip_placeholder=%d\n",
                     static_cast<long long>(applyTimer.elapsed()), sessions.size(),
                     m_list->count(), m_list->verticalScrollBar()->value(),
                     skippedEmptyId, skippedPlaceholder);
        if (!firstSkippedSample.isEmpty())
            std::fprintf(stderr, "APPLY first_skip=%s\n", firstSkippedSample.toUtf8().constData());
        std::fflush(stderr);
    }

    emit sessionsApplied(count());
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
    m_navEmittedId = id;
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
    // 正常情况下这一下点击已经先触发 currentItemChanged 通知过宿主了；
    // 只有「点的是本来就选中的那一项」时 currentItemChanged 不会来，这里补一次。
    if (picked == m_navEmittedId)
        return;
    m_navEmittedId = picked;
    emit sessionSelected(picked);
}

// M14: 选中项变了就切会话（键盘 ↑/↓、鼠标点击、程序化 setCurrentItem 共用）
void SessionListWidget::onCurrentItemChanged(QListWidgetItem *cur, QListWidgetItem *prev)
{
    Q_UNUSED(prev);
    if (!cur)
        return;
    rebuildMenu();
    const QString id = cur->data(kRoleId).toString();
    if (id.isEmpty())    // 分组标题行不是会话
        return;
    if (id == m_navEmittedId)   // 同一个会话反复进过这里，不重复加载
        return;
    m_navEmittedId = id;
    emit sessionSelected(id);
}

// M14: 搜索框按 ↑/↓ → 焦点交给列表并落到最后一个/第一个真会话
bool SessionListWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_searchBox && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Up) {
            if (count() == 0)
                return true;    // 一条结果都没有，就别把焦点扔进空列表
            QListWidgetItem *target = (ke->key() == Qt::Key_Down)
                    ? sessionItem(0) : sessionItem(count() - 1);
            if (!target)
                return true;
            m_list->setCurrentItem(target);
            m_list->setFocus(Qt::OtherFocusReason);
            return true;        // 别让 QLineEdit 把它吃成光标移动
        }
    }
    return QWidget::eventFilter(watched, event);
}
