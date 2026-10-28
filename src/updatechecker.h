#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// 语义化版本比较：a > b 返回正数，a < b 返回负数，相等返回 0
// 容忍 "v" 前缀、"0.5.0-beta.1" 这类输入（预发布后缀不参与比较）
int compareVersions(const QString &a, const QString &b);

// 去掉前缀 v/V 与首尾空白： "v0.6.161" → "0.6.161"
QString normalizeVersion(const QString &raw);

// ============================================================
// 远端仓库最新 Release 查询（GitHub Releases API）
//
// 设计要点（用户要求「GUI 与内核可以分别独立检查更新」）：
//   · 每次 checkLatest 只查一个仓库，实例之间无共享状态
//   · 失败只通过自己的 finished 信号回报，不影响另一个实例
//   · 不缓存、不写配置、不改动任何本地文件
// ============================================================
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    // repo: "raymond202202/flare-client"（owner/name）
    // currentVersion: 本地当前版本（可带 v 前缀）
    // 同一实例同时只允许一个在途请求，重复调用会取消前一个
    void checkLatest(const QString &repo, const QString &currentVersion);

signals:
    // ok=false：message 是可直接展示的失败原因（中文）
    // ok=true ：message 是「已是最新 / 发现新版本」结论
    void finished(bool ok, const QString &latestVersion, const QString &url, const QString &message);

private:
    void onReplyFinished();

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_timeout = nullptr;
    QString m_current;
};

#endif // UPDATECHECKER_H
