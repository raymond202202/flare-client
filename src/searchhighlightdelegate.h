#ifndef SEARCHHIGHLIGHTDELEGATE_H
#define SEARCHHIGHLIGHTDELEGATE_H

#include <QStyledItemDelegate>
#include <QString>

// ============================================================
// 搜索命中高亮（0.6.185 客户端侧）
//
// 分层前提：引擎只给「命中那段话」的纯文本事实（session.preview），
// 「命中在哪」怎么强调是宿主的事。所以这里只画，不查任何东西。
//
// 只在搜索激活时挂上（关键词为空就走基类），平常列表的绘制路径一字不改。
// ============================================================
class SearchHighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SearchHighlightDelegate(QString keyword, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    QString m_keyword;
};

#endif // SEARCHHIGHLIGHTDELEGATE_H
