#ifndef SKILLPANEL_H
#define SKILLPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QJsonObject>
#include <QJsonArray>
#include "engine_bridge.h"

// ============================================================
// 技能/工具面板（M5-3）
// queryTools() 加载 + 展示可用工具/技能：名称 + 描述
// 事件：tools → 刷新列表
// ============================================================
class SkillPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SkillPanel(EngineBridge *bridge, QWidget *parent = nullptr);

    // 测试接口
    QListWidget *list() const { return m_list; }
    QPushButton *refreshButton() const { return m_refreshBtn; }
    int count() const { return m_list->count(); }

public slots:
    void onEngineEvent(const QJsonObject &obj);
    void refresh();

private:
    void applyTools(const QJsonArray &tools);

    EngineBridge *m_bridge;
    QListWidget *m_list;
    QPushButton *m_refreshBtn;
};

#endif // SKILLPANEL_H
