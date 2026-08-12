#ifndef MEMORYPANEL_H
#define MEMORYPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QJsonObject>
#include <QJsonArray>
#include "engine_bridge.h"

// ============================================================
// 记忆查看面板（M3-2）
// get_memories 加载 + 展示：content + type 徽标 + 创建时间
// 事件：memories → 刷新列表
// ============================================================
class MemoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryPanel(EngineBridge *bridge, QWidget *parent = nullptr);

    // 测试接口
    QListWidget *list() const { return m_list; }
    QPushButton *refreshButton() const { return m_refreshBtn; }
    int count() const { return m_list->count(); }

public slots:
    void onEngineEvent(const QJsonObject &obj);
    void refresh();

private:
    void applyMemories(const QJsonArray &memories);

    EngineBridge *m_bridge;
    QListWidget *m_list;
    QPushButton *m_refreshBtn;
};

#endif // MEMORYPANEL_H
