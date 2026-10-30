#include "markdownview.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>
#include <QTextBlock>
#include <QTextBlockFormat>

namespace {

// 按钮尺寸：小一点、贴右上角，像代码块自己的角标而不是盖在代码上的浮层
constexpr int kBtnW = 66;
constexpr int kBtnH = 20;
constexpr int kBtnInset = 10;   // 距右边缘
constexpr int kBtnTopInset = 4; // 距代码块首行顶部

// Qt 的 HTML 导入器给 <pre> 里的每一行都置了 nonBreakableLines。
// 这是实测得到的稳定信号，比「字体是等宽」可靠 —— 正文里也可能写等宽字。
bool isCodeLine(const QTextBlock &block)
{
    return block.isValid() && block.blockFormat().nonBreakableLines();
}

} // namespace

MarkdownView::MarkdownView(QWidget *parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setAcceptRichText(true);
    // 按钮文字要点得到、不会被文本光标抢走焦点
    viewport()->setMouseTracking(true);
    viewport()->setCursor(Qt::IBeamCursor);

    m_resetTimer.setSingleShot(true);
    connect(&m_resetTimer, &QTimer::timeout, this, [this]() {
        m_copied = -1;
        viewport()->update();
    });
}

void MarkdownView::refreshRanges() const
{
    const int revision = document()->revision();
    if (revision == m_rangesRevision)
        return;
    m_rangesRevision = revision;
    m_ranges.clear();

    int first = -1;
    int last = -1;
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        if (isCodeLine(block)) {
            if (first < 0)
                first = block.blockNumber();
            last = block.blockNumber();
        } else if (first >= 0) {
            m_ranges.append({first, last});
            first = -1;
        }
    }
    if (first >= 0)
        m_ranges.append({first, last});
}

QRect MarkdownView::blockRect(int blockNumber) const
{
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return QRect();
    // blockBoundingRect 是文档坐标（含 documentMargin），viewport 里看到的是
    // 它减去滚动量之后的位置。QTextEdit 没有 contentOffset()（那是 QPlainTextEdit
    // 的接口），滚动量直接从滚动条拿。
    const QRectF r = document()->documentLayout()->blockBoundingRect(block);
    const QPoint scroll(horizontalScrollBar()->value(), verticalScrollBar()->value());
    return r.toRect().translated(-scroll.x(), -scroll.y());
}

QVector<QPair<int, int>> MarkdownView::codeRanges() const
{
    refreshRanges();
    return m_ranges;
}

QString MarkdownView::codeText(int index) const
{
    refreshRanges();
    if (index < 0 || index >= m_ranges.size())
        return QString();

    QStringList lines;
    for (int n = m_ranges.at(index).first; n <= m_ranges.at(index).second; ++n) {
        const QTextBlock block = document()->findBlockByNumber(n);
        if (block.isValid())
            lines << block.text();
    }
    // 去掉 pre 首尾可能带的空行 —— 粘到终端里前面多一个空行是常被抱怨的事
    return lines.join(QLatin1Char('\n')).trimmed();
}

QRect MarkdownView::copyButtonRect(int index) const
{
    refreshRanges();
    if (index < 0 || index >= m_ranges.size())
        return QRect();

    const QRect firstLine = blockRect(m_ranges.at(index).first);
    if (firstLine.isNull() || firstLine.isEmpty())
        return QRect();

    int x = viewport()->width() - kBtnW - kBtnInset;
    if (x < firstLine.left() + 4)   // 窗口窄到放不下时贴着块的左边，别跑到块外
        x = firstLine.left() + 4;
    const QRect rect(QPoint(x, firstLine.top() + kBtnTopInset), QSize(kBtnW, kBtnH));
    if (!viewport()->rect().intersects(rect))
        return QRect();   // 滚出可视区，不画也点不到
    return rect;
}

int MarkdownView::rangeAt(const QPoint &pos) const
{
    refreshRanges();
    if (m_ranges.isEmpty())
        return -1;
    const int blockNumber = cursorForPosition(pos).blockNumber();
    for (int i = 0; i < m_ranges.size(); ++i) {
        if (blockNumber >= m_ranges.at(i).first && blockNumber <= m_ranges.at(i).second)
            return i;
    }
    return -1;
}

void MarkdownView::paintEvent(QPaintEvent *event)
{
    QTextEdit::paintEvent(event);

    refreshRanges();
    if (m_ranges.isEmpty())
        return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont btnFont = font();
    btnFont.setPixelSize(11);

    for (int i = 0; i < m_ranges.size(); ++i) {
        const QRect rect = copyButtonRect(i);
        if (rect.isEmpty())
            continue;

        const bool hot = (i == m_hover);
        const bool done = (i == m_copied);

        QColor background(0xf7, 0xf3, 0xec);   // 与代码块底色同调：安静待着
        QColor border(0xef, 0xe6, 0xd2);
        QColor text(0xbf, 0xac, 0x8a);
        if (done) {
            background = QColor(0xff, 0xe8, 0xbf);
            border = QColor(0xe9, 0xd2, 0xa4);
            text = QColor(0x7a, 0x5a, 0x2c);
        } else if (hot) {
            background = QColor(0xff, 0xff, 0xff);
            border = QColor(0xdb, 0xc9, 0xa8);
            text = QColor(0x7a, 0x5c, 0x30);
        }

        painter.setPen(QPen(border, 1));
        painter.setBrush(background);
        painter.drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);

        painter.setFont(btnFont);
        painter.setPen(text);
        painter.drawText(rect, Qt::AlignCenter, done ? QStringLiteral("✓ 已复制")
                                                     : QStringLiteral("复制"));
    }
}

void MarkdownView::mouseMoveEvent(QMouseEvent *event)
{
    const int hover = rangeAt(event->position().toPoint());
    if (hover != m_hover) {
        m_hover = hover;
        viewport()->setCursor(hover >= 0 ? Qt::PointingHandCursor : Qt::IBeamCursor);
        viewport()->update();
    }
    QTextEdit::mouseMoveEvent(event);
}

void MarkdownView::mouseReleaseEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const int index = rangeAt(pos);
    if (event->button() == Qt::LeftButton && index >= 0 && copyButtonRect(index).contains(pos)) {
        const QString code = codeText(index);
        if (!code.isEmpty()) {
            QApplication::clipboard()->setText(code);
            m_copied = index;
            m_resetTimer.start(1200);
            viewport()->update();
            emit codeCopied(index);
        }
        event->accept();
        return;   // 命中按钮就别让基类把光标挪到按钮底下的文字上
    }
    QTextEdit::mouseReleaseEvent(event);
}

void MarkdownView::leaveEvent(QEvent *event)
{
    if (m_hover != -1) {
        m_hover = -1;
        viewport()->update();
    }
    QTextEdit::leaveEvent(event);
}
