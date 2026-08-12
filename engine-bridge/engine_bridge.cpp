#include "engine_bridge.h"

#include <QJsonDocument>
#include <QDebug>

// ============================================================
// 纯函数：协议解析（可单测，不依赖 QProcess）
// ============================================================

QJsonObject parseFlareLine(const QByteArray &line)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();
    return doc.object();
}

QString eventType(const QJsonObject &obj)
{
    return obj.value(QStringLiteral("type")).toString();
}

QByteArray buildRequest(const QJsonObject &req)
{
    return QJsonDocument(req).toJson(QJsonDocument::Compact) + '\n';
}

// ============================================================
// EngineBridge：QProcess 包装
// ============================================================

EngineBridge::EngineBridge(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &EngineBridge::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &EngineBridge::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &EngineBridge::onProcessFinished);
}

EngineBridge::~EngineBridge()
{
    stop();
}

bool EngineBridge::start(const QStringList &args)
{
    if (m_process->state() != QProcess::NotRunning)
        return false;

    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    // 查找 flare 可执行文件（PATH 或 ~/.flare/install）
    m_process->start(QStringLiteral("flare"), args);
    return m_process->waitForStarted(3000);
}

void EngineBridge::stop()
{
    if (m_process->state() == QProcess::NotRunning)
        return;
    m_process->terminate();
    if (!m_process->waitForFinished(2000))
        m_process->kill();
}

bool EngineBridge::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

QString EngineBridge::engineVersion() const
{
    return m_engineVersion;
}

QString EngineBridge::protocolVersion() const
{
    return m_protocolVersion;
}

void EngineBridge::sendRequest(const QJsonObject &req)
{
    emit requestSent(req); // 诊断/测试：无论进程状态都记录请求
    if (m_process->state() != QProcess::Running)
        return;
    m_process->write(buildRequest(req));
}

void EngineBridge::ping()
{
    sendRequest({{QStringLiteral("type"), QStringLiteral("ping")}});
}

void EngineBridge::queryVersion()
{
    sendRequest({{QStringLiteral("type"), QStringLiteral("version")}});
}

void EngineBridge::chat(const QString &sessionId, const QString &input)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("chat")},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("input"), input},
    });
}

void EngineBridge::cancel(const QString &sessionId)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("cancel")},
        {QStringLiteral("sessionId"), sessionId},
    });
}

void EngineBridge::listSessions()
{
    sendRequest({{QStringLiteral("type"), QStringLiteral("list_sessions")}});
}

void EngineBridge::createSession(const QString &sessionId, const QString &title)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("create_session")},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("title"), title},
    });
}

void EngineBridge::deleteSession(const QString &sessionId)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("delete_session")},
        {QStringLiteral("sessionId"), sessionId},
    });
}

void EngineBridge::getMemories(const QString &sessionId)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("get_memories")},
        {QStringLiteral("sessionId"), sessionId},
    });
}

void EngineBridge::confirmResult(const QString &sessionId, const QString &id, const QString &decision)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("confirm_result")},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("id"), id},
        {QStringLiteral("decision"), decision},
    });
}

void EngineBridge::queryModels()
{
    sendRequest({{QStringLiteral("type"), QStringLiteral("models")}});
}

void EngineBridge::onReadyReadStdout()
{
    if (m_processingStdout)
        return; // 重入保护：嵌套事件循环期间的二次读取留给外层继续
    m_processingStdout = true;
    m_stdoutBuffer += m_process->readAllStandardOutput();
    int nl;
    while ((nl = m_stdoutBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_stdoutBuffer.left(nl).trimmed();
        m_stdoutBuffer.remove(0, nl + 1);
        if (!line.isEmpty())
            handleLine(line);
    }
    m_processingStdout = false;
}

void EngineBridge::onReadyReadStderr()
{
    const QByteArray err = m_process->readAllStandardError();
    if (!err.trimmed().isEmpty())
        emit processError(QString::fromUtf8(err.trimmed()));
}

void EngineBridge::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    emit processExited(exitCode);
}

void EngineBridge::handleLine(const QByteArray &line)
{
    const QJsonObject obj = parseFlareLine(line);
    if (obj.isEmpty())
        return; // 非法 JSON 忽略（协议错误可由调用方检测）

    const QString type = eventType(obj);
    if (type == QLatin1String(FlareEvent::Version)) {
        m_engineVersion = obj.value(QStringLiteral("engine")).toString();
        m_protocolVersion = obj.value(QStringLiteral("protocol")).toString();
    }

    emit eventReceived(obj);
}
