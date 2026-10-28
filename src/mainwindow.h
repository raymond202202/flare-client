#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "engine_bridge.h"

class ChatWidget;
class SessionListWidget;
class QLabel;
class QPushButton;

// Flare 客户端主窗口
// UI 铁律：火焰活力色系（橙 #f97316 主色 / 红 #ef4444 / 黄 #fbbf24），干净、轻量
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 测试接口
    EngineBridge *engine() const { return m_engine.get(); }
    SessionListWidget *sessions() const { return m_sessions; }
    // M3-6: 当前模型信息显示文本（状态栏），如 "deepseek-chat · deepseek"
    QString modelInfoText() const { return m_modelInfo; }
    // 状态栏版本文本：客户端版本（编译期）+ 内核名字与版本（运行时 version 帧）
    QString versionText() const;

public slots:
    // 打开记忆查看弹窗（M3-2）
    void openMemoryPanel();
    // M5-3 打开技能/工具面板（弹窗）
    void openSkillPanel();
    // M6-2 打开设置面板（API key 填写弹窗）
    void openSettingsPanel();
    // M3-6: 解析 models 事件，仅展示 model/provider（不泄露 key/baseURL）
    void onEngineEvent(const QJsonObject &obj);
    // 打开「版本与更新」面板（GUI 版本 + 内核名字/版本，两条更新检查互相独立）
    void openVersionPanel();

private:
    std::unique_ptr<EngineBridge> m_engine;
    ChatWidget *m_chat = nullptr;
    SessionListWidget *m_sessions = nullptr;
    QLabel *m_modelLabel = nullptr;
    QPushButton *m_versionButton = nullptr;
    QString m_modelInfo;
    // 来自 version 协议帧（engine / protocol），未连接时为空
    QString m_kernelVersion;
    QString m_protocolVersion;
};

#endif // MAINWINDOW_H
