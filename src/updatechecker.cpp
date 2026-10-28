#include "updatechecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QVector>

// ---- 纯函数：版本号解析/比较（供单测覆盖，不依赖网络）----

QString normalizeVersion(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V')))
        s = s.mid(1);
    return s.trimmed();
}

// 只取数字段，遇到 '-'（预发布）或 '+'（构建元数据）即停止
static QVector<int> versionParts(const QString &raw)
{
    const QString s = normalizeVersion(raw);
    QVector<int> parts;
    QString cur;
    for (const QChar &c : s) {
        if (c.isDigit()) {
            cur += c;
        } else if (c == QLatin1Char('.')) {
            parts.append(cur.isEmpty() ? 0 : cur.toInt());
            cur.clear();
        } else {
            break;
        }
    }
    if (!cur.isEmpty())
        parts.append(cur.toInt());
    if (parts.isEmpty())
        parts.append(0);
    return parts;
}

int compareVersions(const QString &a, const QString &b)
{
    const QVector<int> pa = versionParts(a);
    const QVector<int> pb = versionParts(b);
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa.at(i) : 0;
        const int vb = i < pb.size() ? pb.at(i) : 0;
        if (va != vb)
            return va > vb ? 1 : -1;
    }
    return 0;
}

// ---- 网络查询 ----

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(12000); // 12s 上限，避免按钮一直转
    connect(m_timeout, &QTimer::timeout, this, [this]() {
        if (m_reply)
            m_reply->abort(); // 走 onReplyFinished 的取消分支，统一出口
    });
}

void UpdateChecker::checkLatest(const QString &repo, const QString &currentVersion)
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_current = normalizeVersion(currentVersion);

    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repo));
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "flare-client-update-check");

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::onReplyFinished);
    m_timeout->start();
}

void UpdateChecker::onReplyFinished()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_timeout->stop();
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError nerr = reply->error();

    if (nerr == QNetworkReply::OperationCanceledError) {
        emit finished(false, QString(), QString(),
                      QStringLiteral("检查超时（12 秒无响应）"));
        return;
    }
    if (status == 404) {
        emit finished(false, QString(), QString(),
                      QStringLiteral("该仓库暂无 Release，无法比对"));
        return;
    }
    if (status != 200) {
        if (status == 0) {
            emit finished(false, QString(), QString(),
                          QStringLiteral("网络不可用：%1").arg(reply->errorString()));
        } else if (status == 403) {
            emit finished(false, QString(), QString(),
                          QStringLiteral("检查失败：GitHub 限流或拒绝访问（HTTP 403）"));
        } else {
            emit finished(false, QString(), QString(),
                          QStringLiteral("检查失败：HTTP %1").arg(status));
        }
        return;
    }

    const QByteArray body = reply->readAll();
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit finished(false, QString(), QString(),
                      QStringLiteral("检查失败：响应不是合法 JSON"));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString tag = obj.value(QStringLiteral("tag_name")).toString();
    const QString html = obj.value(QStringLiteral("html_url")).toString();
    const QString latest = normalizeVersion(tag);
    if (latest.isEmpty()) {
        emit finished(false, QString(), html,
                      QStringLiteral("检查失败：远端 Release 缺少版本号"));
        return;
    }

    if (compareVersions(latest, m_current) > 0) {
        emit finished(true, latest, html,
                      QStringLiteral("发现新版本 v%1（当前 v%2）").arg(latest, m_current));
    } else {
        emit finished(true, latest, html,
                      QStringLiteral("已是最新版本（v%1）").arg(m_current));
    }
}
