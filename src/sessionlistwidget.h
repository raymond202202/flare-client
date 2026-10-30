#ifndef SESSIONLISTWIDGET_H
#define SESSIONLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QStringList>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include "engine_bridge.h"

class QMenu;

// ============================================================
// 会话列表侧边栏（M3-1 / M8）
// 加载：recent_sessions（含 preview + updatedAt）/ search_sessions / archived_sessions
// 会话操作：create / rename / end(归档) / restore(恢复) / clear(清空) / delete
//
// M8 变更（用户反馈「归档按钮我认为可以做到目录右键菜单上」）：
//   - 侧边栏的「📦 归档」按钮不再占位，归档 / 置顶 / 重命名 / 恢复 / 清空 / 删除
//     全部收进会话项右键菜单（就地操作，视线不跳走）
//   - 会话名看不出内容 → 标题为「新会话」等占位值时主标题回退显示首条消息预览
//   - 置顶：本地 QSettings 持久化（引擎 pinned 列就绪前先由宿主记住）
// ============================================================
class SessionListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SessionListWidget(EngineBridge *bridge, QWidget *parent = nullptr);

    // 测试接口
    QListWidget *list() const { return m_list; }
    QPushButton *newButton() const { return m_newBtn; }
    QPushButton *deleteButton() const { return m_delBtn; }
    QPushButton *memoryButton() const { return m_memoryBtn; }
    QPushButton *skillButton() const { return m_skillBtn; }
    QPushButton *settingsButton() const { return m_settingsBtn; }
    QLineEdit *searchBox() const { return m_searchBox; }
    // M9: Ctrl+F 聚焦搜索框（并把已有关键字全选，直接输入即覆盖）
    void focusSearch();
    // M8: 归档入口已迁到会话右键菜单；对象保留（不进布局）供测试与无障碍访问
    QPushButton *archiveButton() const { return m_archiveBtn; }
    // M8: 会话项右键菜单（测试可直接读 actions 文本）
    QMenu *contextMenu() const { return m_menu; }
    bool isArchivedView() const { return m_archivedView; }
    QLabel *emptyHint() const { return m_emptyHint; }
    bool isPinned(const QString &sessionId) const;
    QString currentSessionId() const;
    // 只数「真会话」（分组标题行没有 sessionId，不算）
    int count() const;
    // 第 index 个会话项，跳过分组标题行；越界返回 nullptr
    QListWidgetItem *sessionItem(int index) const;
    // M8: 本地别名（该会话首条消息的片段）—— 引擎 title 是占位值时用它，让名字有意义
    void setLocalAlias(const QString &sessionId, const QString &text);

public slots:
    void onEngineEvent(const QJsonObject &event);
    void refresh();
    // M8 视图切换：归档箱 / 最近会话
    void showArchived();   // 打开回收站/归档箱
    void showRecent();     // 返回最近会话列表
    // M8 会话操作（作用于当前选中项）
    void pinCurrent();     // 置顶 / 取消置顶
    void renameCurrent();  // 行内重命名
    void archiveCurrent();   // 归档（end_session）
    void restoreCurrent();   // 从归档恢复（restore_session）
    void clearCurrent();     // 清空消息（clear_session）
    void deleteCurrent();    // M8 回收站：移入回收站（end_session，可恢复）
    void purgeCurrent();     // M8 回收站：彻底删除（delete_session，不可恢复）

signals:
    // 用户点选某个会话（携带协议 sessionId）
    void sessionSelected(const QString &sessionId);
    // 用户点击「记忆」入口
    void memoryRequested();
    // M5-3 用户点击「技能」入口
    void skillRequested();
    // M6-2 用户点击「设置」入口
    void settingsRequested();
    // M8 会话被重命名（宿主可用于同步聊天面板标题）
    void sessionRenamed(const QString &sessionId, const QString &title);

private slots:
    void onCreateClicked();
    void onDeleteClicked();
    void onItemClicked(QListWidgetItem *item);
    void onSearchChanged(const QString &text);
    void onArchiveClicked();
    void onItemChanged(QListWidgetItem *item);
    void onContextMenuRequested(const QPoint &pos);

private:
    void applySessions(const QJsonArray &sessions);
    void rebuildMenu();
    void updateEmptyHint();          // 按当前视图/搜索词刷新空状态文案
    void loadPinned();
    void savePinned();
    void loadAliases();
    void saveAliases();
    QListWidgetItem *currentItem() const;

    EngineBridge *m_bridge;
    QListWidget *m_list;
    QPushButton *m_newBtn;
    QPushButton *m_delBtn;
    QPushButton *m_memoryBtn;
    QPushButton *m_skillBtn;
    QPushButton *m_settingsBtn;
    QLineEdit *m_searchBox;
    QPushButton *m_archiveBtn;
    QMenu *m_menu;
    QLabel *m_emptyHint = nullptr;  // 空列表提示（覆盖在列表区，不参与 count()）

    QStringList m_pinned;         // 置顶会话 id（本地持久化）
    QHash<QString, QString> m_aliases; // 会话 id → 首条消息片段（本地持久化）
    bool m_archivedView = false;  // 当前列表是否为归档箱
    // M8：缓存最近一次列表数据，用于本地别名改写后即时重放（不依赖引擎回执）
    QJsonArray m_lastRecent;
    QJsonArray m_lastArchived;
    bool m_renaming = false;      // 行内重命名进行中
    bool m_suppressItemChanged = false;
    QString m_renamingId;
    QString m_renamingOriginal;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // SESSIONLISTWIDGET_H
