#include "markdownlite.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

// 代码块：浅米底 + 等宽 + 圆角（和工具卡片同一套米色系，不刺眼）
QString codeBlockHtml(const QString &code)
{
    return QStringLiteral(
               "<pre style=\"background:#f7f3ec;border:1px solid #eadfc8;border-radius:6px;"
               "padding:6px 8px;margin:6px 0;font-family:monospace;\">%1</pre>")
        .arg(code.toHtmlEscaped());
}

// 单行内联标记：`code`、**粗体**、*斜体*、[文字](链接)
QString inlineHtml(QString line)
{
    // 先整体转义，后面的正则都在转义后的文本上跑（* 和 ` 不受转义影响）
    QString s = line.toHtmlEscaped();

    // 行内代码优先，避免里面的 * 被当粗斜体
    s.replace(QRegularExpression(QStringLiteral("`([^`]+)`")),
              QStringLiteral("<code style=\"background:#f1ede5;padding:1px 3px;"
                             "border-radius:3px;font-family:monospace;\">\\1</code>"));

    s.replace(QRegularExpression(QStringLiteral("\\*\\*([^*\\n]+)\\*\\*")),
              QStringLiteral("<b>\\1</b>"));

    s.replace(QRegularExpression(QStringLiteral("(^|[^*])\\*([^*\\n]+)\\*(?!\\*)")),
              QStringLiteral("\\1<i>\\2</i>"));

    s.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\((https?://[^)\\s]+)\\)")),
              QStringLiteral("<a href=\"\\2\">\\1</a>"));

    return s;
}

} // namespace

QString MarkdownLite::toHtml(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    QString out;
    QString codeBuf;
    bool inCode = false;

    const QRegularExpression headingRe(QStringLiteral("^(#{1,6})\\s+(.*)$"));
    const QRegularExpression bulletRe(QStringLiteral("^[-*+]\\s+(.*)$"));

    for (const QString &raw : lines) {
        const QString trimmed = raw.trimmed();

        if (trimmed.startsWith(QStringLiteral("```"))) {
            if (inCode) {
                out += (out.isEmpty() ? QString() : QStringLiteral("<br/>")) + codeBlockHtml(codeBuf);
                codeBuf.clear();
                inCode = false;
            } else {
                inCode = true;
            }
            continue;
        }

        if (inCode) {
            codeBuf += raw + QLatin1Char('\n');
            continue;
        }

        if (trimmed.isEmpty())
            continue; // 空行不再额外加 <br/>，行间已有一个

        QString line;
        const auto heading = headingRe.match(trimmed);
        if (heading.hasMatch()) {
            line = QStringLiteral("<b style=\"font-size:15px;\">%1</b>")
                       .arg(inlineHtml(heading.captured(2)));
        } else {
            const auto bullet = bulletRe.match(trimmed);
            if (bullet.hasMatch())
                line = QStringLiteral("• ") + inlineHtml(bullet.captured(1));
            else
                line = inlineHtml(trimmed);
        }

        if (!out.isEmpty())
            out += QStringLiteral("<br/>");
        out += line;
    }

    if (inCode && !codeBuf.isEmpty()) {
        const QString block = codeBlockHtml(codeBuf);
        // 未闭合的代码块也要显示出来（模型偶尔会忘记收尾）
        out += (out.isEmpty() ? QString() : QStringLiteral("<br/>")) + block;
    }

    return out;
}
