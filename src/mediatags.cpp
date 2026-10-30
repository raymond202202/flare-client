#include "mediatags.h"

#include <QJsonValue>
#include <QStringList>

namespace MediaTags {

QString label(const QJsonArray &kinds)
{
    QStringList tags;
    for (const QJsonValue &v : kinds) {
        const QString k = v.toString();
        if (k == QLatin1String("image")) {
            if (!tags.contains(QStringLiteral("[图片]")))
                tags << QStringLiteral("[图片]");
        } else if (k == QLatin1String("file")) {
            if (!tags.contains(QStringLiteral("[附件]")))
                tags << QStringLiteral("[附件]");
        }
    }
    return tags.join(QLatin1Char(' '));
}

QString withText(const QString &text, const QJsonArray &kinds)
{
    const QString head = label(kinds);
    const QString body = text.trimmed();
    if (head.isEmpty())
        return body;
    return body.isEmpty() ? head : head + QStringLiteral(" ") + body;
}

} // namespace MediaTags
