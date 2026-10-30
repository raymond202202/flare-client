#ifndef ENGINE_BRIDGE_H
#define ENGINE_BRIDGE_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QQueue>
#include <functional>

// ============================================================
// Flare 引擎桥接层（M1）
// QProcess spawn `flare server`，通过 stdin/stdout JSON Lines 通信
//
// 协议要点（flare src/server.ts, HOST_PROTOCOL_VERSION=1.0）：
//   请求：stdin 每行一个 JSON 对象，必须有 "type" 字段
//   响应：stdout 每行一个 JSON 对象（事件流）
//   chat 命令事件：text / tool_call / tool_result / done / error / cancelled
//   其他命令响应：ok / pong / version / sessions / usage / ...
// ============================================================

// ---- 事件类型（与 flare 协议对齐）----
namespace FlareEvent {
inline const char *Text        = "text";
inline const char *ToolCall    = "tool_call";
inline const char *ToolResult  = "tool_result";
inline const char *Done        = "done";
inline const char *Error       = "error";
inline const char *Cancelled   = "cancelled";
inline const char *Ok          = "ok";
inline const char *Pong        = "pong";
inline const char *Version     = "version";
inline const char *Sessions    = "sessions";
inline const char *Memories    = "memories";
inline const char *Models      = "models";
inline const char *Confirm     = "confirm";
inline const char *ToolExecute = "tool_execute";
inline const char *Tools       = "tools";
inline const char *RecentSessions = "recent_sessions";
inline const char *Messages     = "messages";
} // namespace FlareEvent

// 解析一行 stdout JSON → QJsonObject（非法 JSON 返回空对象）
QJsonObject parseFlareLine(const QByteArray &line);

// 从 QJsonObject 提取事件类型（无 type 字段返回空串）
QString eventType(const QJsonObject &obj);

// 构造协议请求（单行 JSON）
QByteArray buildRequest(const QJsonObject &req);

// 引擎二进制候选路径，按优先级排列（可单测的纯逻辑）。
// 为什么不能只看「与本程序同目录」：生产安装时本程序在 /usr/bin，
// 引擎在 /usr/lib64/flare-client/，同目录永远找不到 —— 结果就是装好的
// 正式包一启动就报「引擎启动失败」。凡是「注释里写了路径」的地方，
// 代码里就必须真的去查那个路径。
QStringList engineCandidates();

// 从候选里挑第一个「存在且可执行」的；一个都没有返回空串（调用方退回 PATH）
QString pickEngine(const QStringList &candidates);

class EngineBridge : public QObject
{
    Q_OBJECT

public:
    explicit EngineBridge(QObject *parent = nullptr);
    ~EngineBridge() override;

    // 启动 flare server 子进程（spawn 命令可注入，默认 "flare server"）
    bool start(const QStringList &args = {"server"});
    void stop();

    bool isRunning() const;
    QString engineVersion() const;   // 最近一次 version 响应
    QString protocolVersion() const; // 最近一次 version 响应的 protocol

    // M8 工作目录（用户反馈：没有选择工作目录 / 读取本地文件的入口）
    // 引擎的工具沙箱根硬编码 os.Getwd()（internal/tools 里 os.Getwd()），没有 cwd 参数，
    // 因此宿主侧用 QProcess 的工作目录把根目录带过去 —— 零协议改动即可生效。
    // 改目录后需重启引擎进程（stop + start）才对工具可见。
    void setWorkingDirectory(const QString &dir);
    QString workingDirectory() const { return m_workdir; }
    // 引擎子进程 pid（<=0 表示没起来）。自检要读 /proc/<pid>/cwd 证明
    // 「引擎真的以所选目录为根」，而不是只信界面上的文案。
    qint64 enginePid() const { return m_process ? m_process->processId() : 0; }

signals:
    // 每个 stdout 事件（含 chat 流式事件），obj = 完整 JSON
    void eventReceived(const QJsonObject &obj);
    // 每条发出的协议请求（测试/诊断用）
    void requestSent(const QJsonObject &req);
    void processExited(int exitCode);
    void processError(const QString &message);

public slots:
    // 发送任意协议请求（序列化单行 JSON）
    void sendRequest(const QJsonObject &req);
    // 便捷命令
    void ping();
    void queryVersion();
    void chat(const QString &sessionId, const QString &input);
    void cancel(const QString &sessionId = "default");
    void listSessions();
    // M5-4 拉取最近会话（含 preview 首条消息预览 + updatedAt 时间；响应 recent_sessions）
    // limit>0 时请求引擎按该上限返回；引擎默认只给最近 20 条，
    // 再多就得靠翻页把上限抬高（列表滚到底时调用）
    void recentSessions(int limit = 0);
    // M7-2 按关键词搜索会话（响应 search_sessions；只读，不触发生成）
    void searchSessions(const QString &query, int limit = 20);
    // M7-2 拉取归档会话列表（响应 archived_sessions；只读）
    void archivedSessions();
    void createSession(const QString &sessionId, const QString &title);
    void deleteSession(const QString &sessionId);
    // M8 会话管理（协议 §23/§24/…：引擎侧早已实现，宿主此前未封装）
    // 重命名会话：ok 回执带 title（空白标题引擎会回 error，宿主侧先 Trim 判空）
    void renameSession(const QString &sessionId, const QString &title);
    // 归档会话：end_session，ok 回执 archived=true（归档后从最近列表消失）
    void endSession(const QString &sessionId);
    // 从归档恢复：restore_session，ok 回执 restored=true
    void restoreSession(const QString &sessionId);
    // 清空会话消息但保留会话记录与用量：clear_session，ok 回执 cleared=N
    void clearSession(const QString &sessionId);
    void getMemories(const QString &sessionId = "default");
    // M3-5 确认门回传：decision ∈ allow_once/allow_session/always/deny/alternative
    void confirmResult(const QString &sessionId, const QString &id, const QString &decision);
    // M5-5 读取会话历史消息（响应 messages 事件；recent=true 返回最近 limit 条）
    void getMessages(const QString &sessionId, int limit = 50);
    // M3-6 查询模型信息（响应 models 事件；只读，不触发生成）
    void queryModels();
    // M5-3 查询可用工具/技能（响应 tools 事件；只读）
    void queryTools();

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void handleLine(const QByteArray &line);

    QProcess *m_process;
    QByteArray m_stdoutBuffer;
    bool m_processingStdout = false; // 重入保护：嵌套事件循环期间阻止二次解析
    QString m_engineVersion;
    QString m_protocolVersion;
    QString m_workdir;               // M8 工具沙箱根（QProcess 工作目录）
};

#endif // ENGINE_BRIDGE_H
