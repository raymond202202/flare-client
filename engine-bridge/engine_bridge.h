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
} // namespace FlareEvent

// 解析一行 stdout JSON → QJsonObject（非法 JSON 返回空对象）
QJsonObject parseFlareLine(const QByteArray &line);

// 从 QJsonObject 提取事件类型（无 type 字段返回空串）
QString eventType(const QJsonObject &obj);

// 构造协议请求（单行 JSON）
QByteArray buildRequest(const QJsonObject &req);

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
    void createSession(const QString &sessionId, const QString &title);
    void deleteSession(const QString &sessionId);
    void getMemories(const QString &sessionId = "default");
    // M3-5 确认门回传：decision ∈ allow_once/allow_session/always/deny/alternative
    void confirmResult(const QString &sessionId, const QString &id, const QString &decision);
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
};

#endif // ENGINE_BRIDGE_H
