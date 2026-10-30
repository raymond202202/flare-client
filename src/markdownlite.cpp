#include "markdownlite.h"

#include <QList>
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

// --- 表格 ---------------------------------------------------------------
// 模型回答「对比 A 和 B」时几乎总给表格。不收表格的话整段会变成一行行
// 竖线文本，比不渲染还难读。

// GFM 表格行：首尾都有 |，至少一个格子（`| a | b |`，也接受单列 `| a |`）
bool looksLikeTableRow(const QString &t)
{
    return t.size() >= 3 && t.startsWith(QLatin1Char('|')) && t.endsWith(QLatin1Char('|'))
           && t.count(QLatin1Char('|')) >= 2;
}

// 分隔行：只由 | - : 空格组成，且至少一个 -（`|---|:--:|`）。
// 必须见到分隔行才认成表格 —— 否则含竖线的正文（shell 管道之类）会被误吞。
bool looksLikeTableSeparator(const QString &t)
{
    if (!looksLikeTableRow(t))
        return false;
    bool hasDash = false;
    for (const QChar c : t) {
        if (c == QLatin1Char('-')) {
            hasDash = true;
            continue;
        }
        if (c == QLatin1Char('|') || c == QLatin1Char(':') || c.isSpace())
            continue;
        return false;
    }
    return hasDash;
}

QStringList splitTableRow(const QString &t)
{
    const QString inner = t.mid(1, t.size() - 2); // 去掉首尾 |
    QStringList cells = inner.split(QLatin1Char('|'));
    for (QString &c : cells)
        c = c.trimmed();
    return cells;
}

// Qt 富文本支持 table/cellpadding/cellspacing/bgcolor，但不支持
// border-collapse 与 border-color，所以表头用底色区分、网格线交给 border=1。
QString tableHtml(const QList<QStringList> &rows)
{
    if (rows.isEmpty())
        return QString();

    int cols = 0;
    for (const QStringList &r : rows)
        cols = qMax(cols, r.size());

    QString h = QStringLiteral(
        "<table cellspacing=\"0\" cellpadding=\"7\" border=\"1\" width=\"100%\">");
    for (int i = 0; i < rows.size(); ++i) {
        const bool header = (i == 0);
        const QStringList &row = rows.at(i);
        h += QStringLiteral("<tr>");
        for (int c = 0; c < cols; ++c) {
            const QString cell = c < row.size() ? row.at(c) : QString();
            if (header) {
                h += QStringLiteral("<td bgcolor=\"#f7f3ec\"><b>%1</b></td>")
                         .arg(inlineHtml(cell));
            } else {
                h += QStringLiteral("<td>%1</td>").arg(inlineHtml(cell));
            }
        }
        h += QStringLiteral("</tr>");
    }
    h += QStringLiteral("</table>");
    return h;
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

    // 用索引循环：表格要往后看一行（分隔行）才能确认，range-for 做不到
    for (int i = 0; i < lines.size(); ++i) {
        const QString raw = lines.at(i);
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

        // 表格：本行像表头，且下一行是 |---|---| 分隔行
        if (looksLikeTableRow(trimmed) && i + 1 < lines.size()
            && looksLikeTableSeparator(lines.at(i + 1).trimmed())) {
            QList<QStringList> rows;
            rows << splitTableRow(trimmed);
            i += 2; // 跳过表头与分隔行
            while (i < lines.size() && looksLikeTableRow(lines.at(i).trimmed())) {
                rows << splitTableRow(lines.at(i).trimmed());
                ++i;
            }
            --i; // 外层 for 还会 ++i
            if (!out.isEmpty())
                out += QStringLiteral("<br/>");
            out += tableHtml(rows);
            continue;
        }

        // 列表：连续的 -/*/+ 行合成一个真 <ul>。原来用「• 」前缀塞进同一个
        // 段落，长条目一折行第二行就顶到最左边，看着像两个条目。
        if (bulletRe.match(trimmed).hasMatch()) {
            QString items;
            while (i < lines.size()) {
                const QString cur = lines.at(i).trimmed();
                if (cur.isEmpty())
                    break;
                const auto item = bulletRe.match(cur);
                if (!item.hasMatch())
                    break;
                items += QStringLiteral("<li>%1</li>").arg(inlineHtml(item.captured(1)));
                ++i;
            }
            --i; // 外层 for 还会 ++i
            if (!out.isEmpty())
                out += QStringLiteral("<br/>");
            out += QStringLiteral("<ul style=\"margin:0;\">%1</ul>").arg(items);
            continue;
        }

        QString line;
        const auto heading = headingRe.match(trimmed);
        if (heading.hasMatch()) {
            line = QStringLiteral("<b style=\"font-size:15px;\">%1</b>")
                       .arg(inlineHtml(heading.captured(2)));
        } else {
            // 连续 bullet 行已在上面的分支里合成 <ul>，走到这里的一定是正文。
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
