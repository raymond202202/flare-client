#ifndef SESSIONLISTWIDGET_H
#define SESSIONLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QJsonObject>
#include <QJsonArray>
#include "engine_bridge.h"

// ============================================================
// 会话列表侧边栏（M3-1）
// list_sessions 加载会话 + 新建（create_session）/切换/删除（delete_session）
// 事件：sessions → 刷新列表；ok（create/delete 回执）→ 重新拉取
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
    QString currentSessionId() const;
    int count() const { return m_list->count(); }

public slots:
    void onEngineEvent(const QJsonObject &obj);
    void refresh();

signals:
    // 用户点选某个会话（携带协议 sessionId）
    void sessionSelected(const QString &sessionId);
    // 用户点击「记忆」入口
    void memoryRequested();
    // M5-3 用户点击「技能」入口
    void skillRequested();

private slots:
    void onCreateClicked();
    void onDeleteClicked();
    void onItemClicked(QListWidgetItem *item);

private:
    void applySessions(const QJsonArray &sessions);

    EngineBridge *m_bridge;
    QListWidget *m_list;
    QPushButton *m_newBtn;
    QPushButton *m_delBtn;
    QPushButton *m_memoryBtn;
    QPushButton *m_skillBtn;
};

#endif // SESSIONLISTWIDGET_H
