#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QTextEdit>

#include "markdownview.h"
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>
#include <QHash>
#include <QDateTime>
#include <QUrl>
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
    // M10 拖放：把本地文件拖进窗口即挂成附件（拖放事件本身是 protected，
    // 这里暴露一个可测入口，dragEnter/drop 都走它）
    void attachLocalFiles(const QList<QUrl> &urls);
    QStringList attachments() const { return m_attachments; }
    // 剪贴板里是图片就落盘挂成附件，返回是否已当附件处理（不是图片返回 false）
    bool pasteImageFromClipboard();
    QString outputText() const { return m_output->toPlainText(); }
    // 用户是否已经站在最新位置（可以收起「↓ 新内容」提示）。
    // maximum 在追加内容、布局重算的瞬间可能归零——那不是「回到底部」，是范围还没算出来。
    static bool atLatestPosition(int value, int maximum);
    QString sessionId() const { return m_sessionId; }
    // 首启引导是否显示中（测试与上层逻辑用）
    bool suggestVisible() const;
    // 收起欢迎态与首启引导（开始对话时调用；加载到非空历史也会自动收起）
    void dismissWelcome();
    // 引擎级故障（进程没起来 / 启动失败）要摆在对话区，不能只闪一下状态栏
    void showEngineNotice(const QString &text);
    // 草稿按会话隔离：切走再切回来，打的字和挂的附件都还在；
    // 更重要的是，在 A 会话打的字不会跟着跑到 B 会话被误发出去。
    struct Draft {
        QString text;
        QStringList files;
    };

signals:

    // 把文件夹拖进窗口 ≠ 想把它当附件发出去（引擎读不出目录内容），
    // 更自然的含义是「在这个目录下干活」→ 抛给 MainWindow 切工作目录
    void directoryDropRequested(const QString &dir);

public slots:
    void sendMessage();
    void onEngineEvent(const QJsonObject &obj);
    // 切换会话：更新目标会话 id，清空消息区避免串会话
    void setSession(const QString &sessionId);
    // M3-5 确认门回传：弹窗允许/拒绝后调用（测试可直接调，跳过弹窗）
    void respondConfirm(const QString &id, const QString &decision);
    // M4-5 易用性：停止当前生成（发 cancel 协议）
    void stopGeneration();
    // M5-5 历史消息渲染（引擎 messages 事件进来后逐条画到消息区）
    void renderHistory(const QJsonArray &messages);

protected:
    // M8 粘贴图片：QLineEdit 不支持富内容，在输入框上拦 Ctrl+V，
    // 剪贴板有图片就存成 png 附件，否则走默认文本粘贴。
    bool eventFilter(QObject *watched, QEvent *event) override;
    // M10 拖放：从文件管理器把文件拖进来 = 附件（用户反馈要「读取本地文件的入口」，
    // 拖放是比点 📎 更自然的入口）
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QHash<QString, Draft> m_drafts;
    // 智能跟随滚动：用户正在往上翻读旧消息时，新内容不能把他拽回底部；
    // 只有本来就在底部（贴着最新消息）才自动跟随。
    bool nearBottom() const;
    void scrollToBottom();
    // 上翻读历史时新内容进来：底部浮出「↓ 新内容」提示，点一下跳回最新。
    // 两个都不能做：硬把用户拽回底部（打断阅读）、什么都不提示（以为卡死了）。
    void followOrHint(bool atBottom);
    void positionJumpHint();
    // M8 附件（本地文件 / 剪贴板图片）
    void pickFiles();
    // M10 拖放高亮：拖到面板上时消息区描边变橙，松手还原
    void setDropHighlight(bool on);
    void addAttachment(const QString &path);
    void removeAttachment(const QString &path);
    void rebuildAttachChips();
    // M15-3：stamp 为空表示不显示时间（系统提示不挂时间戳）
    void appendMessage(const QString &who, const QString &text,
                       const QString &stamp = QString());
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
    // M15-4：历史里的 Flare 回复同样要过 MarkdownLite —— 打开旧会话看到的是
    // 排版好的答案（代码块、列表、表格），不是原始 markdown 文本。
    void appendRenderedReply(const QString &markdown, const QString &stamp = QString());
    // M15-5：把渲染结果写进文档。以代码块/列表/表格开头的回答必须先断开段落，
    // 否则块的第一行会被吸进「Flare：」那一行（既没底色也不在复制范围里）。
    static void insertMarkdown(QTextCursor &cursor, const QString &markdown);
    // M6-4 串会话防护：事件带 sessionId 时校验是否属于当前会话
    // （引擎所有会话事件均带 sessionId；不带的老协议/测试视为放行）
    bool eventForSession(const QJsonObject &obj) const;
    // M3-5 confirm 事件 → 弹窗（允许/拒绝），按选择回传 confirm_result
    void showConfirmDialog(const QJsonObject &confirmEvent);
    // M3-5 重入安全：不在事件处理栈内弹模态框；缓存后延迟到下一事件循环 tick
    QJsonObject m_pendingConfirm;
    bool m_confirmScheduled = false;

    EngineBridge *m_bridge;
    MarkdownView *m_output;   // M15：带代码块「复制」按钮的消息区
    QLineEdit *m_input;
    QPushButton *m_sendBtn;
    QPushButton *m_stopBtn = nullptr;
    QString m_sessionId;
    bool m_streaming = false; // 是否处于 AI 回复流中（chunk 追加目标块）
    // M9: 本轮流式正文在文档里的起始位置（endStream 时把这段纯文本换成渲染后的富文本）
    int m_streamStartPos = -1;
    // M15-3：本轮回复的开始时刻（= 这条消息的时间，收流后打在末尾）
    QDateTime m_streamStartedAt;
    QTimer *m_breathTimer = nullptr; // M5-2 欢迎词呼吸动画
    bool m_welcomeVisible = false;   // 欢迎词是否显示中

    // M12 首启起点 chip：欢迎页只有海报、没有下一步，用户会愣在那里。
    // 三个可点的起点，点一下填进输入框（不代你发送），发出第一条就收起。
    QWidget *m_suggestBar = nullptr;
    void setSuggestVisible(bool visible);

    // M8 附件区（本地文件 + 剪贴板图片），默认隐藏，有附件才显示
    QPushButton *m_attachBtn = nullptr;
    QWidget *m_attachBar = nullptr;
    QHBoxLayout *m_attachChips = nullptr;
    QStringList m_attachments;       // 待发送附件的绝对路径
    QPushButton *m_jumpBtn = nullptr; // 「↓ 新内容」浮标（挂在 m_output 上绝对定位）
};

#endif // CHATWIDGET_H
