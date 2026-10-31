#include "searchhighlightdelegate.h"
#include "flametheme.h"

#include <QPainter>
#include <cstdio>
#include <cstdlib>
#include <QStyleOptionViewItem>
#include <QFontMetrics>

namespace {

// 画一行：把 keyword 的每一处命中先铺底色再描字，其余段落正常色。
// 逐段推进而不是整体 drawText，是为了让底色只覆盖命中那几个字。
void drawLine(QPainter *p, const QRect &rect, const QFont &font, const QString &text,
              const QString &keyword, const QColor &fg, const QColor &hlBg, const QColor &hlFg)
{
    const QFontMetrics fm(font);
    p->setFont(font);
    int x = rect.left();
    QString rest = text;
    // 文本已在宿主侧按像素裁好，这里只兜底防死循环
    int guard = 0;
    while (!rest.isEmpty() && guard++ < 200) {
        const int i = rest.indexOf(keyword, 0, Qt::CaseInsensitive);
        if (i < 0) {
            p->setPen(fg);
            p->drawText(QRect(x, rect.top(), rect.right() - x + 1, rect.height()),
                        Qt::AlignLeft | Qt::AlignVCenter, rest);
            return;
        }
        if (i > 0) {
            const QString plain = rest.left(i);
            p->setPen(fg);
            p->drawText(QRect(x, rect.top(), rect.right() - x + 1, rect.height()),
                        Qt::AlignLeft | Qt::AlignVCenter, plain);
            x += fm.horizontalAdvance(plain);
        }
        const QString hit = rest.mid(i, keyword.size());
        const int w = fm.horizontalAdvance(hit);
        p->save();
        p->setPen(Qt::NoPen);
        p->setBrush(hlBg);
        p->drawRoundedRect(QRectF(x - 1, rect.top() + 1, w + 2, rect.height() - 2), 3, 3);
        p->restore();
        p->setPen(hlFg);
        p->drawText(QRect(x, rect.top(), w + 2, rect.height()),
                    Qt::AlignLeft | Qt::AlignVCenter, hit);
        x += w;
        rest = rest.mid(i + keyword.size());
    }
}

} // namespace

SearchHighlightDelegate::SearchHighlightDelegate(QString keyword, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_keyword(keyword.trimmed())
{
}

void SearchHighlightDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    static const bool dbg = qEnvironmentVariableIsSet("FLARE_DEBUG_LIST");
    if (dbg) {
        std::fprintf(stderr, "[HL] kw=%s text=%s\n", m_keyword.toUtf8().constData(),
                     opt.text.left(40).replace(QLatin1Char('\n'), QLatin1Char('/')).toUtf8().constData());
        std::fflush(stderr);
    }

    const QString text = opt.text;
    // 没有关键词，或者这一项压根不含关键词（时间戳副行、分组标题）→ 原样交给基类
    if (m_keyword.isEmpty() || text.isEmpty()
        || !text.contains(m_keyword, Qt::CaseInsensitive)) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // 先让基类把背景、悬停、选中、焦点框都画好，但别让它画文字：
    // 清空 opt.text 后自己描一遍，才能逐字上底色。
    opt.text.clear();
    QStyledItemDelegate::paint(painter, opt, index);

    const bool selected = opt.state & QStyle::State_Selected;
    const QColor mainFg = selected ? opt.palette.color(QPalette::HighlightedText)
                                   : opt.palette.color(QPalette::Text);
    QColor softFg = selected ? opt.palette.color(QPalette::HighlightedText) : FlameTheme::textSoft();
    if (selected)
        softFg.setAlpha(200);
    // 命中底色用琥珀（火焰主题的过渡色），半透明压在选中底色上也不糊
    // 透明度不能再低了：浅底上 alpha 太低的高亮跟没画一样（实测 70 只剩
    // 一层几乎看不出的米黄）。140 才能压出真正的琥珀块，同时不遮住底色文字。
    QColor hlBg = FlameTheme::yellow();
    hlBg.setAlpha(selected ? 165 : 140);
    const QColor hlFg = FlameTheme::dark();

    const QRect area = opt.rect.adjusted(6, 2, -6, -2);
    const QFontMetrics fm(opt.font);
    const QStringList lines = text.split(QLatin1Char('\n'));

    painter->save();
    painter->setClipRect(area);
    const int lineH = fm.height();
    const int totalH = lineH * qMin<int>(lines.size(), 2);
    int y = area.top() + (area.height() - totalH) / 2;
    for (int i = 0; i < lines.size() && i < 2; ++i) {
        const QRect lineRect(area.left(), y, area.width(), lineH);
        drawLine(painter, lineRect, opt.font, lines.at(i), m_keyword,
                 i == 0 ? mainFg : softFg, hlBg, i == 0 ? hlFg : hlFg);
        y += lineH;
    }
    painter->restore();
}
