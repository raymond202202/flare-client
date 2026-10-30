#include "flaretime.h"

#include <QRegularExpression>
#include <QTimeZone>

namespace {

// 串里是否已经写了时区（Z / +08:00 / +0800）
bool hasZone(const QString &iso)
{
    static const QRegularExpression re(QStringLiteral("(Z|z|[+-]\\d{2}:?\\d{2})$"));
    return re.match(iso).hasMatch();
}

// 没写时区的串：按 UTC 解释（引擎/数据库都存 UTC）
QDateTime fromBareUtc(const QString &iso)
{
    static const char *const formats[] = {
        "yyyy-MM-dd HH:mm:ss.zzz", "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-ddTHH:mm:ss.zzz", "yyyy-MM-ddTHH:mm:ss",
        "yyyy-MM-dd HH:mm",        "yyyy-MM-dd",
    };
    for (const char *f : formats) {
        const QDateTime dt = QDateTime::fromString(iso, QString::fromLatin1(f));
        if (dt.isValid())
            return QDateTime(dt.date(), dt.time(), QTimeZone::UTC);
    }
    return QDateTime();
}

QDateTime effectiveNow(const QDateTime &now)
{
    return now.isValid() ? now : QDateTime::currentDateTime();
}

} // namespace

QDateTime FlareTime::parse(const QString &iso)
{
    const QString t = iso.trimmed();
    if (t.isEmpty())
        return QDateTime();

    QDateTime dt;
    if (hasZone(t))
        dt = QDateTime::fromString(t, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = fromBareUtc(t);
    if (!dt.isValid())
        dt = QDateTime::fromString(t, Qt::ISODateWithMs);
    if (!dt.isValid())
        return QDateTime();
    return dt.toLocalTime();
}

QString FlareTime::listStamp(const QString &iso, const QDateTime &now)
{
    const QDateTime dt = parse(iso);
    if (!dt.isValid())
        return iso; // 宁可显示怪串，也别把时间清成 1970
    const QDate today = effectiveNow(now).date();
    if (dt.date() == today)
        return dt.toString(QStringLiteral("HH:mm"));
    if (dt.date().year() == today.year())
        return dt.toString(QStringLiteral("MM-dd"));
    return dt.toString(QStringLiteral("yyyy-MM-dd"));
}

QString FlareTime::section(const QString &iso, const QDateTime &now)
{
    const QDateTime dt = parse(iso);
    if (!dt.isValid())
        return QStringLiteral("更早");
    const QDate today = effectiveNow(now).date();
    const QDate d = dt.date();
    if (d == today)
        return QStringLiteral("今天");
    if (d == today.addDays(-1))
        return QStringLiteral("昨天");
    if (d >= today.addDays(-6))
        return QStringLiteral("最近 7 天");
    return QStringLiteral("更早");
}

QString FlareTime::messageStampAt(const QDateTime &when, const QDateTime &now)
{
    if (!when.isValid())
        return QString();
    const QDateTime local = (when.timeSpec() == Qt::LocalTime) ? when : when.toLocalTime();
    const QDate today = effectiveNow(now).date();
    if (local.date() == today)
        return local.toString(QStringLiteral("HH:mm"));
    if (local.date() == today.addDays(-1))
        return QStringLiteral("昨天 ") + local.toString(QStringLiteral("HH:mm"));
    if (local.date().year() == today.year())
        return local.toString(QStringLiteral("MM-dd HH:mm"));
    return local.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString FlareTime::messageStamp(const QString &iso, const QDateTime &now)
{
    const QDateTime dt = parse(iso);
    if (!dt.isValid())
        return QString();
    return messageStampAt(dt, now);
}

QString FlareTime::fullStamp(const QString &iso)
{
    const QDateTime dt = parse(iso);
    if (!dt.isValid())
        return iso;
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}
