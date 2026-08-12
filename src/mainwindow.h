#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "engine_bridge.h"

class ChatWidget;
class SessionListWidget;
class QLabel;

// Flare 客户端主窗口
// UI 铁律：浅色白底紫配（主色 #6d4aff），干净、轻量
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

public slots:
    // 打开记忆查看弹窗（M3-2）
    void openMemoryPanel();
    // M3-6: 解析 models 事件，仅展示 model/provider（不泄露 key/baseURL）
    void onEngineEvent(const QJsonObject &obj);

private:
    std::unique_ptr<EngineBridge> m_engine;
    ChatWidget *m_chat = nullptr;
    SessionListWidget *m_sessions = nullptr;
    QLabel *m_modelLabel = nullptr;
    QString m_modelInfo;
};

#endif // MAINWINDOW_H
