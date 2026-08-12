#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "engine_bridge.h"

class ChatWidget;
class SessionListWidget;

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

private slots:
    // 打开记忆查看弹窗（M3-2）
    void openMemoryPanel();

private:
    std::unique_ptr<EngineBridge> m_engine;
    ChatWidget *m_chat = nullptr;
    SessionListWidget *m_sessions = nullptr;
};

#endif // MAINWINDOW_H
