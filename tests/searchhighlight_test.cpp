#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QListWidget>
#include <QPixmap>
#include "../src/searchhighlightdelegate.h"

// 搜索命中高亮：命中词必须在列表项里真的被「涂色」，
// 否则引擎辛苦给出的命中上下文（preview）在视觉上等于没给。
// 用像素统计验证，而不是看代码写得对不对 —— 这套界面回归靠截图证据。
static int yellowishPixels(const QImage &img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.red() - c.blue() > 15 && c.red() > 200)
                ++n;
        }
    }
    return n;
}

class SearchHighlightTest : public QObject
{
    Q_OBJECT
private slots:
    void highlightsKeyword();
    void plainRowHasNoHighlight();
};

void SearchHighlightTest::highlightsKeyword()
{
    QListWidget list;
    list.resize(220, 60);
    list.setItemDelegate(new SearchHighlightDelegate(QStringLiteral("介绍你自己"), &list));
    auto *item = new QListWidgetItem(QStringLiteral("用一句话介绍你自己"), &list);
    item->setSizeHint(QSize(220, 52));
    list.show();

    const QImage img = list.viewport()->grab().toImage();
    QVERIFY2(yellowishPixels(img) > 20, "命中词没有被涂上高亮底色");
}

void SearchHighlightTest::plainRowHasNoHighlight()
{
    // 关键词跟这一项无关（例如分组标题、时间戳副行）：干净交给基类，不许乱涂
    QListWidget list;
    list.resize(220, 60);
    list.setItemDelegate(new SearchHighlightDelegate(QStringLiteral("完全无关"), &list));
    auto *item = new QListWidgetItem(QStringLiteral("用一句话介绍你自己"), &list);
    item->setSizeHint(QSize(220, 52));
    list.show();

    const QImage img = list.viewport()->grab().toImage();
    QVERIFY2(yellowishPixels(img) < 5, "不含关键词的行不该出现高亮底色");
}

QTEST_MAIN(SearchHighlightTest)
#include "searchhighlight_test.moc"
