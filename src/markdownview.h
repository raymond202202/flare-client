#ifndef MARKDOWNVIEW_H
#define MARKDOWNVIEW_H

#include <QPair>
#include <QTextEdit>
#include <QTimer>
#include <QVector>

// ============================================================
// MarkdownView —— 消息区：在代码块右上角挂一个「复制」按钮
//
// 为什么需要这个类：模型回复里代码块占比很高，想把一段代码拿走，只能
// 在 QTextEdit 里手动拖选 —— 跨行等宽文本很容易多选或少选一行，粘出去
// 才发现少了最后一行。代码块又恰好是最该被原样复制的东西。
//
// 职责划分：Markdown → HTML 仍归 MarkdownLite，这里不重复解析。只做两件事：
//   1. 认出代码块。Qt 的 HTML 导入器把 <pre> 的每一行拆成一个 block，并给
//      blockFormat().nonBreakableLines() 置 true（实测验证：正文段落不会带
//      这个标记；靠字体名/背景色判断都会误伤正文里的等宽字体）。相邻的
//      这类 block 合并成一段 —— 也就是用户眼里「一个代码块」。
//   2. 每段右上角画一个「复制」按钮，点一下把这段代码原样写进剪贴板。
//
// 按钮常驻而不是 hover 才出现：藏着没人知道有这功能。非 hover 时用低对比
// 配色（和代码块底色几乎同调），鼠标进到哪一段哪一段才亮起来。
// ============================================================
class MarkdownView : public QTextEdit
{
    Q_OBJECT
public:
    explicit MarkdownView(QWidget *parent = nullptr);

    // 文档里每个代码块的首尾 block 号（闭区间），按出现顺序
    QVector<QPair<int, int>> codeRanges() const;
    // 第 index 个代码块的正文（块之间 \n 连接，去掉首尾空行）
    QString codeText(int index) const;
    // 第 index 个代码块「复制」按钮在 viewport 坐标系里的矩形；
    // 为空 = 这一段不在可视区（画不出来，也点不到）
    QRect copyButtonRect(int index) const;
    // 刚被点过的块（按钮显示「已复制」，1.2s 后复位）
    int copiedRange() const { return m_copied; }
    // 鼠标当前所在的代码块（-1 = 不在任何代码块里）
    int hoveredRange() const { return m_hover; }

signals:
    void codeCopied(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void refreshRanges() const;
    QRect blockRect(int blockNumber) const;
    int rangeAt(const QPoint &pos) const;

    mutable QVector<QPair<int, int>> m_ranges;
    mutable int m_rangesRevision = -1;
    int m_hover = -1;
    int m_copied = -1;
    QTimer m_resetTimer;
};

#endif // MARKDOWNVIEW_H
