#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "engine_bridge.h"

class ChatWidget;
class SessionListWidget;
class QLabel;
class QPushButton;
class QSystemTrayIcon;
class QCloseEvent;

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
    ChatWidget *chat() const { return m_chat; }
    void restoreWindowGeometry();
    void saveWindowGeometry();
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
    // 引擎上报的「主模型有没有 key」：-1 未知 / 0 没有 / 1 有
    int engineHasApiKey() const { return m_engineHasApiKey; }
    // M3-6: 解析 models 事件，仅展示 model/provider（不泄露 key/baseURL）
    void onEngineEvent(const QJsonObject &obj);
    // 打开「版本与更新」面板（GUI 版本 + 内核名字/版本，两条更新检查互相独立）
    void openVersionPanel();
    // M8: 选择工作目录（= 引擎工具读写本地文件的根目录），换完自动重启引擎
    void chooseWorkingDirectory();
    // 选目录对话框与「把文件夹拖进窗口」共用同一段落盘 + 重启引擎逻辑
    void applyWorkingDirectory(const QString &dir);
    // 引擎断开后的原地重启入口：点了就重新拉起引擎，对话记录不动。
    // 不能让用户为了恢复只能重开客户端 —— 那等于直接丢掉正在看的上下文。
    void restartEngine();

protected:
    // M8: 关闭窗口默认不退出 —— 有托盘就收进托盘，没托盘就最小化
    void closeEvent(QCloseEvent *event) override;

private:
    void setupTray();
    // 状态栏引擎状态：连接成功/断开两态，断开时可点重启
    void setEngineConnected(bool connected);
    // 状态按钮两套配色：绿=正常、琥珀=断开可点。颜色是这里唯一的语义载体，别省。
    void applyEngineButtonStyle(bool connected);
    QString workdirText() const;
    QString settingsFilePath() const;

    std::unique_ptr<EngineBridge> m_engine;
    ChatWidget *m_chat = nullptr;
    SessionListWidget *m_sessions = nullptr;
    QLabel *m_modelLabel = nullptr;
    QPushButton *m_versionButton = nullptr;
    QPushButton *m_workdirButton = nullptr;
    QPushButton *m_engineButton = nullptr;   // 引擎状态 + 重启入口（状态栏常驻）
    // 首次使用没有 API Key 时的状态栏常驻提示（可点开设置）
    QPushButton *m_keyHintButton = nullptr;
    int m_engineHasApiKey = -1;            // 引擎上报的主模型 key 状态（三态）
    bool m_keySavedNoticePending = false;  // 刚从设置面板回来，等 hasApiKey 翻转后道一声
    // 问引擎要一次 models（key 状态由引擎判定，宿主不自己解析 .env）
    void checkApiKeyConfigured();
    // 按引擎上报的 hasApiKey 增删状态栏常驻提示
    void updateKeyHint();
    QSystemTrayIcon *m_tray = nullptr;
    bool m_forceQuit = false;   // 托盘「退出」置位，closeEvent 才真正放行
public:
    // 托盘里到底有没有图标：--close-test 自检要能回答「关闭后用户找不找得回来」
    bool trayVisible() const;
private:
    QString m_modelInfo;
    // 来自 version 协议帧（engine / protocol），未连接时为空
    QString m_kernelVersion;
    QString m_protocolVersion;
};

#endif // MAINWINDOW_H
