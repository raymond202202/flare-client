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
    // keyword 为空时 indexOf("") 恒返回 0：下面的高亮循环会原地打转 200 次
    // 而一个字都不画（非搜索态整行空白）。空关键词直接整段绘制。
    if (keyword.isEmpty()) {
        p->setPen(fg);
        p->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
        return;
    }
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
        std::fprintf(stderr, "[HL] rect=%dx%d kw=%s text=%s\n", opt.rect.width(), opt.rect.height(),
                     m_keyword.toUtf8().constData(),
                     QString(opt.text).replace(QChar(0x2028), QLatin1Char('/'))
                         .replace(QChar(0x2029), QLatin1Char('/')).left(60).toUtf8().constData());
        std::fflush(stderr);
    }

    // Qt 把文本交给 delegate 之前会把 '\n' 规范化成 U+2028（行分隔符）：
    // 直接 split('\n') 只得到一行，副行的时间被压进主行再被 elide 裁掉，
    // 同一标题的两条会话便完全同形。这里先还原成真正的换行。
    QString text = opt.text;
    text.replace(QChar(0x2028), QLatin1Char('\n'));
    text.replace(QChar(0x2029), QLatin1Char('\n'));

    const bool hit = !m_keyword.isEmpty() && text.contains(m_keyword, Qt::CaseInsensitive);
    // 右对齐的时间列：宿主把它单独放在 kFlareRoleTime 里，不再混进第二行。
    // 连续多条同名会话里，只有这一列是竖向齐的，扫一眼就能比对出差异。
    const QString timeStr = index.data(kFlareRoleTime).toString();
    const bool hasTime = !timeStr.isEmpty();
    // 单行项（分组标题等）交给基类；双行项必须自绘 —— 基类 paint 内部会重新
    // initStyleOption，把 opt.text 又换回 U+2028 的单行串，副行照样丢失。
    if (text.isEmpty() || (!hasTime && !text.contains(QLatin1Char('\n')))) {
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
    // 时间列原来用 textSoft（很浅的灰），在密密麻麻的同名会话里几乎看不见，
    // 一排「14:36」糊成灰带，反而加重重影感 —— 时间列要比副行更实。
    if (!selected)
        softFg = mainFg;
    if (selected)
        softFg.setAlpha(200);
    // 命中底色用琥珀（火焰主题的过渡色），半透明压在选中底色上也不糊
    // 透明度不能再低了：浅底上 alpha 太低的高亮跟没画一样（实测 70 只剩
    // 一层几乎看不出的米黄）。140 才能压出真正的琥珀块，同时不遮住底色文字。
    QColor hlBg = FlameTheme::yellow();
    hlBg.setAlpha(selected ? 165 : 140);
    if (!hit)
        hlBg.setAlpha(0);   // 常规列表没有命中词，别给副行铺底色
    const QColor hlFg = FlameTheme::dark();

    const QRect area = opt.rect.adjusted(8, 2, -12, -2);
    // 用 painter 自己的 fontMetrics：QFontMetrics(font) 是拿默认 DPI 新建的，
    // painter 若带 device transform / DPI 缩放，算出来的宽度比实际绘制窄几个像素，
    // elide 就会「以为放得下」而把字画进时间列 —— 这正是标题和时间叠在一起的原因。
    painter->setFont(opt.font);
    const QFontMetrics fm(painter->fontMetrics());
    const QStringList lines = text.split(QLatin1Char('\n'));

    painter->save();
    painter->setClipRect(area);
    const int lineH = fm.height();
    const int rows = qMin<int>(lines.size(), 2);
    // 两行之间留 2px 呼吸位：紧贴时第二行（淡色副行）看起来像第一行的影子。
    const int lineGap = 2;
    const int totalH = rows > 1 ? lineH * 2 + lineGap : lineH;
    const int y0 = area.top() + (area.height() - totalH) / 2;
    // 时间列带宽 = 文字宽 + 左侧留白 30px：右对齐时多出来的留白全落在标题
    // 与时间之间，标题因此提前收口，两栏之间才有明显空地（实测 +10 只剩 5px）。
    const int timeW = hasTime ? fm.horizontalAdvance(timeStr) + 20 : 0;
    // 标题可用宽度必须扣掉时间列：单行布局里两者共处一行，不让位就直接压字。
    // 与时间列之间留 20px：实测 8px 时标题末字与时间首字只隔 5px，扫视像连成
    // 一句；留到 20px 后视觉空隙约 11px，标题与时间一眼分成两栏。
    const int titleW = hasTime ? qMax(24, area.width() - timeW - 20) : area.width();

    // 时间画在「最后一行」的右端：标题独占第一行才不会被时间挤成半截，
    // 而所有条目的时间都贴同一条右边缘，竖向扫一眼就是一列可比的刻度。
    if (hasTime) {
        const int timeRowY = rows > 1 ? y0 + lineH + lineGap : y0;
        const QRect timeRect(area.right() - timeW + 1, timeRowY, timeW, lineH);
        painter->setFont(opt.font);
        painter->setPen(softFg);
        painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, timeStr);
    }

    // 条目文本是全文，这里按「让出时间列后」的实际宽度截断：侧栏变宽 → 同一条
    // 会话能显示更多字，用户拖宽侧栏立刻见效，不用等宿主重建列表。
    const QString shownTitle = fm.elidedText(lines.at(0), Qt::ElideRight, titleW);
    // 除了 elide，再夹一层硬裁切：elide 依赖字体测量，测量与实际渲染差 1-2px
    // 就够把标题尾巴压进时间列（用户看到的「文字叠在一起」）。裁切是几何保证。
    painter->save();
    painter->setClipRect(QRect(area.left(), y0, titleW, lineH).adjusted(0, 0, -1, 0));
    drawLine(painter, QRect(area.left(), y0, titleW, lineH), opt.font, shownTitle,
             m_keyword, mainFg, hlBg, hlFg);
    painter->restore();
    if (rows > 1) {
        QRect secondRect(area.left(), y0 + lineH + lineGap, titleW, lineH);
        painter->save();
        painter->setClipRect(secondRect.adjusted(0, 0, -1, 0));
        drawLine(painter, secondRect, opt.font, lines.at(1), m_keyword, softFg, hlBg, hlFg);
        painter->restore();
    }
    painter->restore();
}
