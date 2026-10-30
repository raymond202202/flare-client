// M15：消息区代码块「复制」按钮
//
// 守的是什么：模型回复里的代码块必须能被一键原样拿走。
// 反证方式（改坏会立刻红）：
//   - 把 isCodeLine() 的 nonBreakableLines 判定去掉 → codeRanges() 变空，
//     codeBlockSurvivesMarkdownLite / copyButtonCopiesVerbatim 全红；
//   - 把 mouseReleaseEvent 里的 copyButtonRect(index).contains(pos) 去掉 →
//     clickingCodeButNotButtonDoesNotCopy 红（点代码正文也会被当复制）。

#include <QApplication>
#include <QClipboard>
#include <QTextBlock>
#include <QtTest>

#include "markdownlite.h"
#include "markdownview.h"

namespace {

// 一段带代码块的回复，跟引擎实际吐出来的形状一致
QString replyWithCode()
{
    return QStringLiteral("我先看下仓库状态：\n\n"
                          "```\n"
                          "cd ~/hermes-projects/flare-client\n"
                          "git status\n"
                          "make -j8\n"
                          "```\n"
                          "上面三行跑完就能看到结果。");
}

void pump()
{
    QCoreApplication::processEvents();
}

} // namespace

class MarkdownViewTest : public QObject
{
    Q_OBJECT

private slots:
    void codeBlockSurvivesMarkdownLite();
    void copyButtonCopiesVerbatim();
    void clickingCodeButNotButtonDoesNotCopy();
    void twoCodeBlocksAreIndependent();
    void inlineCodeIsNotACodeBlock();
    void appendedReplyKeepsCopyButton();
};

// 渲染链路：MarkdownLite 吐 HTML → QTextDocument 导入 → 我们能把代码块找回来
void MarkdownViewTest::codeBlockSurvivesMarkdownLite()
{
    MarkdownView view;
    view.resize(640, 420);
    view.setHtml(MarkdownLite::toHtml(replyWithCode()));
    pump();

    const auto ranges = view.codeRanges();
    QCOMPARE(ranges.size(), 1);
    QCOMPARE(view.codeText(0), QStringLiteral("cd ~/hermes-projects/flare-client\ngit status\nmake -j8"));
    // 代码块的行不能在正文里也冒出来（说明它真的被隔离成一段）
    QVERIFY(!view.toPlainText().contains(QStringLiteral("```")));
}

// 点按钮 → 剪贴板拿到的是代码原文，一个字不多一个字不少
void MarkdownViewTest::copyButtonCopiesVerbatim()
{
    MarkdownView view;
    view.resize(640, 420);
    view.show();
    view.setHtml(MarkdownLite::toHtml(replyWithCode()));
    pump();

    const QRect button = view.copyButtonRect(0);
    QVERIFY2(!button.isEmpty(), "代码块右上角应该有一个可点的复制按钮");
    QVERIFY(view.viewport()->rect().contains(button));

    QApplication::clipboard()->setText(QStringLiteral("<sentinel>"));
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, button.center());
    pump();

    QCOMPARE(QApplication::clipboard()->text(),
             QStringLiteral("cd ~/hermes-projects/flare-client\ngit status\nmake -j8"));
    QCOMPARE(view.copiedRange(), 0);   // 按钮必须马上切成「已复制」
}

// 点代码正文（不是按钮）不能动剪贴板 —— 否则用户想选中一行时会被偷走内容
void MarkdownViewTest::clickingCodeButNotButtonDoesNotCopy()
{
    MarkdownView view;
    view.resize(640, 420);
    view.show();
    view.setHtml(MarkdownLite::toHtml(replyWithCode()));
    pump();

    const QRect button = view.copyButtonRect(0);
    QVERIFY(!button.isEmpty());

    QApplication::clipboard()->setText(QStringLiteral("<sentinel>"));
    // 同行最左边：确实落在代码块内，但离按钮很远
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(6, button.center().y()));
    pump();

    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("<sentinel>"));
    QCOMPARE(view.copiedRange(), -1);
}

// 一段回复里两个代码块，各复制各的，不能串
void MarkdownViewTest::twoCodeBlocksAreIndependent()
{
    MarkdownView view;
    view.resize(640, 520);
    view.show();
    view.setHtml(MarkdownLite::toHtml(QStringLiteral("先备份：\n\n```\ncp a.db a.db.bak\n```\n"
                                                     "然后清理：\n\n```\nrm -f *.tmp\n```\n"
                                                     "就这两步。")));
    pump();

    const auto ranges = view.codeRanges();
    QCOMPARE(ranges.size(), 2);
    QCOMPARE(view.codeText(0), QStringLiteral("cp a.db a.db.bak"));
    QCOMPARE(view.codeText(1), QStringLiteral("rm -f *.tmp"));

    const QRect second = view.copyButtonRect(1);
    QVERIFY(!second.isEmpty());
    QApplication::clipboard()->setText(QStringLiteral("<sentinel>"));
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, second.center());
    pump();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("rm -f *.tmp"));
    QCOMPARE(view.copiedRange(), 1);
}

// 行内 `代码`（单反引号）不是代码块：不给按钮，不然每条回复上都会冒出小按钮
void MarkdownViewTest::inlineCodeIsNotACodeBlock()
{
    MarkdownView view;
    view.resize(640, 300);
    view.show();
    view.setHtml(MarkdownLite::toHtml(QStringLiteral("用 `git status` 看下当前改动，"
                                                     "再用 `git diff` 看具体内容。")));
    pump();

    QCOMPARE(view.codeRanges().size(), 0);
    // 正文里的等宽字体不能因为「看着像代码」就被当成代码块
    QVERIFY(view.copyButtonRect(0).isEmpty());
}

// appendMessage() 走的是 QTextEdit::append，不是 setHtml —— 这条路径也得有按钮
void MarkdownViewTest::appendedReplyKeepsCopyButton()
{
    MarkdownView view;
    view.resize(640, 460);
    view.show();
    view.setHtml(MarkdownLite::toHtml(QStringLiteral("第一轮：我先看下仓库状态。")));
    view.append(MarkdownLite::toHtml(replyWithCode()));
    pump();

    QCOMPARE(view.codeRanges().size(), 1);
    const QRect button = view.copyButtonRect(0);
    QVERIFY2(!button.isEmpty(), "append 插入的代码块同样要给出复制按钮");

    QApplication::clipboard()->setText(QStringLiteral("<sentinel>"));
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, button.center());
    pump();
    QCOMPARE(QApplication::clipboard()->text(),
             QStringLiteral("cd ~/hermes-projects/flare-client\ngit status\nmake -j8"));
}

QTEST_MAIN(MarkdownViewTest)
#include "markdownview_test.moc"
