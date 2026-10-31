#include "engine_bridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>

// ============================================================
// 引擎查找（可单测：不启动进程，只算路径）
// ============================================================

QStringList engineCandidates()
{
#ifdef Q_OS_WIN
    const QString engineName = QStringLiteral("flare.exe");
#else
    const QString engineName = QStringLiteral("flare");
#endif
    QStringList list;

    // 1) FLARE_ENGINE_BIN —— 显式指定优先。自动化测试在 build 目录跑时用得上，
    //    高级用户换了自编译引擎也靠它，显式配置就该赢。
    const QString explicitEngine = QString::fromLocal8Bit(qgetenv("FLARE_ENGINE_BIN"));
    if (!explicitEngine.isEmpty())
        list << explicitEngine;

    // 2) 与本程序同目录 —— 便携版、Windows 安装包、macOS .app 内都是这个布局
    const QString appDir = QCoreApplication::applicationDirPath();
    list << appDir + QLatin1Char('/') + engineName;

#ifdef Q_OS_MAC
    // .app 里引擎可以放 Resources（codesign 更省事）
    list << appDir + QStringLiteral("/../Resources/") + engineName;
#endif

#ifndef Q_OS_WIN
    // 3) 系统安装位置 —— RPM（/usr/lib64）与 deb（/usr/lib，含 multiarch）。
    //    包管理器把引擎和 GUI 分开放在 lib 目录，绝不是 /usr/bin 同目录：
    //    只查同目录 = 正式装完就用不了。
    list << QStringLiteral("/usr/lib64/flare-client/flare")
         << QStringLiteral("/usr/lib/flare-client/flare")
         << QStringLiteral("/usr/lib/x86_64-linux-gnu/flare-client/flare")
         << QStringLiteral("/usr/lib/aarch64-linux-gnu/flare-client/flare");
    // 4) 用户级安装（装到自家目录的引擎，不碰系统目录）
    list << QDir::homePath() + QStringLiteral("/.flare/install/flare");
#endif

    // 5) 最后退回 PATH —— 开发环境
    const QString onPath = QStandardPaths::findExecutable(engineName);
    if (!onPath.isEmpty())
        list << onPath;

    return list;
}

QString pickEngine(const QStringList &candidates)
{
    for (const QString &c : candidates) {
        if (c.isEmpty())
            continue;
        const QFileInfo fi(c);
        if (fi.exists() && fi.isExecutable())
            return c;
    }
    return QString();
}

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
    // M8: 把工作目录交给引擎进程 —— 引擎的工具沙箱根就是 os.Getwd()，
    // 没有 cwd 参数可用，这是零协议改动下让「选择工作目录 / 读本地文件」生效的唯一途径。
    if (!m_workdir.isEmpty() && QFileInfo(m_workdir).isDir())
        m_process->setWorkingDirectory(m_workdir);
    // 查找 flare 引擎：候选路径见 engineCandidates()（按优先级）。
    // 一个都没找到时退回 "flare"，让 QProcess 的失败信息如实暴露问题，
    // 而不是我们在这里编一个「请确认已安装」的模糊提示。
    QString program = pickEngine(engineCandidates());
    if (program.isEmpty())
        program = QStringLiteral("flare");
    m_process->start(program, args);
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

// M8: 记录工具沙箱根目录。进程在跑时不会立即生效——由调用方（主窗口）负责
// stop + start 重启，重启后引擎就以新目录作为工具根。
void EngineBridge::setWorkingDirectory(const QString &dir)
{
    // 空字符串 = 恢复默认（继承 GUI 自身 cwd）；非目录一律拒绝，避免静默无效
    if (dir.isEmpty()) {
        m_workdir.clear();
        return;
    }
    const QFileInfo info(dir);
    if (!info.exists() || !info.isDir())
        return;
    m_workdir = info.absoluteFilePath();
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

void EngineBridge::recentSessions(int limit)
{
    QJsonObject req{{QStringLiteral("type"), QStringLiteral("recent_sessions")}};
    if (limit > 0)
        req.insert(QStringLiteral("limit"), limit);
    sendRequest(req);
}

void EngineBridge::searchSessions(const QString &query, int limit, bool archived)
{
    QJsonObject req{{QStringLiteral("type"), QStringLiteral("search_sessions")}};
    if (!query.trimmed().isEmpty())
        req.insert(QStringLiteral("query"), query.trimmed());
    req.insert(QStringLiteral("limit"), limit);
    // 只在回收站视图下显式声明范围，主列表是默认口径，不往协议里塞冗余字段
    if (archived)
        req.insert(QStringLiteral("archived"), true);
    sendRequest(req);
}

void EngineBridge::archivedSessions()
{
    // 协议命令是 list_archived_sessions（v0.6.31），响应事件 type=archived_sessions
    sendRequest({{QStringLiteral("type"), QStringLiteral("list_archived_sessions")}});
}

void EngineBridge::getMessages(const QString &sessionId, int limit)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("get_messages")},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("recent"), true},
        {QStringLiteral("limit"), limit},
    });
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

// M8 会话管理四件套：协议字段名与引擎 compat.go 逐字对齐
// （sessionId 缺省时引擎按 "default" 处理，这里一律显式传）
void EngineBridge::renameSession(const QString &sessionId, const QString &title)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("rename_session")},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("title"), title},
    });
}

void EngineBridge::endSession(const QString &sessionId)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("end_session")},
        {QStringLiteral("sessionId"), sessionId},
    });
}

void EngineBridge::restoreSession(const QString &sessionId)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("restore_session")},
        {QStringLiteral("sessionId"), sessionId},
    });
}

void EngineBridge::clearSession(const QString &sessionId)
{
    sendRequest({
        {QStringLiteral("type"), QStringLiteral("clear_session")},
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

void EngineBridge::queryTools()
{
    sendRequest({{QStringLiteral("type"), QStringLiteral("tools")}});
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
    // 防滞留：嵌套事件循环（如 confirm 弹窗）期间到达的数据若未被读取，
    // 缓冲可能仍有不完整行；补一个延迟读取避免数据永久滞留。
    if (m_process->bytesAvailable() > 0)
        QTimer::singleShot(0, this, &EngineBridge::onReadyReadStdout);
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
