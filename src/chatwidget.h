#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
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
    QString outputText() const { return m_output->toPlainText(); }
    QString sessionId() const { return m_sessionId; }

public slots:
    void sendMessage();
    void onEngineEvent(const QJsonObject &obj);
    // 切换会话：更新目标会话 id，清空消息区避免串会话
    void setSession(const QString &sessionId);

private:
    void appendMessage(const QString &who, const QString &text);
    // M3-3 流式渲染：text 事件按 chunk 增量追加到当前消息块
    void appendStreamChunk(const QString &chunk);
    void endStream();

    EngineBridge *m_bridge;
    QTextEdit *m_output;
    QLineEdit *m_input;
    QPushButton *m_sendBtn;
    QString m_sessionId;
    bool m_streaming = false; // 是否处于 AI 回复流中（chunk 追加目标块）
};

#endif // CHATWIDGET_H
