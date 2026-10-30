#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>
#include <memory>
#include "engine_bridge.h"
// ============================================================
// 聊天面板（M2 最小可用 UI）
// 消息流 + 输入框 + 发送按钮；通过 EngineBridge 与 flare server 通信
// ============================================================
class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(EngineBridge *bridge, QWidget *parent = nullptr);

    // 测试接口
    QLineEdit *input() const { return m_input; }
    QTextEdit *output() const { return m_output; }
    QPushButton *stopButton() const { return m_stopBtn; }
    QPushButton *attachButton() const { return m_attachBtn; }
    QPushButton *sendButton() const { return m_sendBtn; }
    QWidget *attachBar() const { return m_attachBar; }
    QStringList attachments() const { return m_attachments; }
    QString outputText() const { return m_output->toPlainText(); }
    QString sessionId() const { return m_sessionId; }

signals:
    // M8 用户反馈「会话名看不出这个会话是干什么的」：
    // 首条消息发出后把内容片段抛给会话列表，列表用它作为该会话的本地别名。
    // （引擎侧自动标题要改数据层，先用宿主别名让名字立刻有意义）
    void firstMessageSent(const QString &sessionId, const QString &text);

public slots:
    void sendMessage();
    void onEngineEvent(const QJsonObject &obj);
    // 切换会话：更新目标会话 id，清空消息区避免串会话
    void setSession(const QString &sessionId);
    // M3-5 确认门回传：弹窗允许/拒绝后调用（测试可直接调，跳过弹窗）
    void respondConfirm(const QString &id, const QString &decision);
    // M4-5 易用性：停止当前生成（发 cancel 协议）
    void stopGeneration();

protected:
    // M8 粘贴图片：QLineEdit 不支持富内容，在输入框上拦 Ctrl+V，
    // 剪贴板有图片就存成 png 附件，否则走默认文本粘贴。
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // M8 附件（本地文件 / 剪贴板图片）
    void pickFiles();
    void addAttachment(const QString &path);
    void removeAttachment(const QString &path);
    void rebuildAttachChips();
    void appendMessage(const QString &who, const QString &text);
    // M3-3 流式渲染：text 事件按 chunk 增量追加到当前消息块
    void appendStreamChunk(const QString &chunk);
    void endStream();
    // M3-4 工具调用/结果卡片式展示（浅色紫底卡片）
    void appendToolCard(const QString &icon, const QString &title, const QString &body);
    // M5-2 跃动欢迎词：会话开始展示火焰渐变 F L A R E + 呼吸动画
    void showWelcomeBanner();
    void startWelcomeBreathing();
    void stopWelcomeBreathing();
    void updateWelcomeBreath();
    // M5-5 加载会话历史（get_messages → messages 事件 → 渲染）
    void loadHistory(const QString &sessionId);
    void renderHistory(const QJsonArray &messages);
    // M6-4 串会话防护：事件带 sessionId 时校验是否属于当前会话
    // （引擎所有会话事件均带 sessionId；不带的老协议/测试视为放行）
    bool eventForSession(const QJsonObject &obj) const;
    // M3-5 confirm 事件 → 弹窗（允许/拒绝），按选择回传 confirm_result
    void showConfirmDialog(const QJsonObject &confirmEvent);
    // M3-5 重入安全：不在事件处理栈内弹模态框；缓存后延迟到下一事件循环 tick
    QJsonObject m_pendingConfirm;
    bool m_confirmScheduled = false;

    EngineBridge *m_bridge;
    QTextEdit *m_output;
    QLineEdit *m_input;
    QPushButton *m_sendBtn;
    QPushButton *m_stopBtn = nullptr;
    QString m_sessionId;
    bool m_streaming = false; // 是否处于 AI 回复流中（chunk 追加目标块）
    // M9: 本轮流式正文在文档里的起始位置（endStream 时把这段纯文本换成渲染后的富文本）
    int m_streamStartPos = -1;
    QTimer *m_breathTimer = nullptr; // M5-2 欢迎词呼吸动画
    bool m_welcomeVisible = false;   // 欢迎词是否显示中

    // M8 附件区（本地文件 + 剪贴板图片），默认隐藏，有附件才显示
    QPushButton *m_attachBtn = nullptr;
    QWidget *m_attachBar = nullptr;
    QHBoxLayout *m_attachChips = nullptr;
    QStringList m_attachments;       // 待发送附件的绝对路径
    bool m_firstMessageDone = false; // 本会话是否已发过消息（别名只取首条）
};

#endif // CHATWIDGET_H
