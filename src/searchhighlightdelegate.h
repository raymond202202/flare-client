#ifndef SEARCHHIGHLIGHTDELEGATE_H
#define SEARCHHIGHLIGHTDELEGATE_H

#include <QStyledItemDelegate>
#include <QString>

// 会话项携带的「时间」事实（已由宿主格式化好的短时间串）。
// 空表示这条不显示时间。放在头文件里是为了让列表与测试用同一个角色号 ——
// 两处各写一份常量，早晚会错位成一个查不到数据的空列。
constexpr int kFlareRoleTime = Qt::UserRole + 3;

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
