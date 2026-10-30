#include <QtTest>
#include <QString>

#include "markdownlite.h"

// ============================================================
// MarkdownLite —— 富文本渲染单元测试
//
// 这些用例都来自真实使用中的丑陋输出：
//   · 模型给「对比 A 和 B」的表格 → 之前会变成一行行竖线文本
//   · 代码块缩进/尖括号丢失
//   · 没闭合的代码块直接消失
// ============================================================
class MarkdownLiteTest : public QObject
{
    Q_OBJECT

private slots:
    // 表格必须变成真表格，而不是原样吐竖线
    void tableBecomesRealTable()
    {
        const QString md = QStringLiteral("| 发行版 | 包管理 |\n"
                                          "| --- | --- |\n"
                                          "| Fedora | dnf |\n"
                                          "| Ubuntu | apt |\n");
        const QString html = MarkdownLite::toHtml(md);

        QVERIFY(html.contains(QStringLiteral("<table")));
        QCOMPARE(html.count(QStringLiteral("<tr>")), 3);  // 表头 + 2 行
        QVERIFY(html.contains(QStringLiteral("Fedora")));
        QVERIFY(html.contains(QStringLiteral("dnf")));
        // 表头加粗，和正文数据行区分开
        QVERIFY(html.contains(QStringLiteral("<b>发行版</b>")));
        QVERIFY(!html.contains(QStringLiteral("<b>Fedora</b>")));
        // 分隔行本身不能出现在输出里
        QVERIFY(!html.contains(QStringLiteral("---")));
    }

    // 单元格里的内联格式也要生效
    void formattingInsideCells()
    {
        const QString md = QStringLiteral("| 项 | 说明 |\n"
                                          "| --- | --- |\n"
                                          "| **必填** | 跑 `dnf install` |\n");
        const QString html = MarkdownLite::toHtml(md);

        QVERIFY(html.contains(QStringLiteral("<b>必填</b>")));
        QVERIFY(html.contains(QStringLiteral("<code")));
        QVERIFY(html.contains(QStringLiteral("dnf install")));
    }

    // 列数不一致也不能崩，缺的格子留空
    void raggedTablePadsCells()
    {
        const QString md = QStringLiteral("| a | b | c |\n"
                                          "| - | - | - |\n"
                                          "| 1 |\n");
        const QString html = MarkdownLite::toHtml(md);
        QVERIFY(html.contains(QStringLiteral("<table")));
        QCOMPARE(html.count(QStringLiteral("<tr>")), 2);
    }

    // 没有分隔行的竖线文本不算表格：shell 管道之类不能被吞掉
    void pipesWithoutSeparatorStayText()
    {
        const QString md = QStringLiteral("| grep foo | sort\n"
                                          "| 只是正文里带竖线 |\n");
        const QString html = MarkdownLite::toHtml(md);
        QVERIFY(!html.contains(QStringLiteral("<table")));
        QVERIFY(html.contains(QStringLiteral("grep foo")));
    }

    // 代码块：保留换行与缩进，尖括号转义，等宽
    void codeBlockKeepsIndentation()
    {
        const QString md = QStringLiteral("看这个：\n"
                                          "```bash\n"
                                          "if true; then\n"
                                          "    echo \"<hi>\"\n"
                                          "fi\n"
                                          "```\n");
        const QString html = MarkdownLite::toHtml(md);

        QVERIFY(html.contains(QStringLiteral("<pre")));
        QVERIFY(html.contains(QStringLiteral("font-family:monospace")));
        QVERIFY(html.contains(QStringLiteral("    echo")));   // 4 空格缩进还在
        QVERIFY(html.contains(QStringLiteral("&lt;hi&gt;"))); // 尖括号已转义
        QVERIFY(!html.contains(QStringLiteral("<hi>")));
    }

    // 模型偶尔忘记收尾，未闭合的代码块也要显示出来而不是消失
    void unclosedCodeBlockStillRendered()
    {
        const QString md = QStringLiteral("```\necho hi\n");
        const QString html = MarkdownLite::toHtml(md);
        QVERIFY(html.contains(QStringLiteral("<pre")));
        QVERIFY(html.contains(QStringLiteral("echo hi")));
    }

    // 表格后面紧跟普通段落，不能被吸进表格
    void textAfterTableSurvives()
    {
        const QString md = QStringLiteral("| a | b |\n"
                                          "| - | - |\n"
                                          "| 1 | 2 |\n"
                                          "结论：都用 dnf。\n");
        const QString html = MarkdownLite::toHtml(md);
        QVERIFY(html.contains(QStringLiteral("<table")));
        QVERIFY(html.contains(QStringLiteral("结论")));
        QCOMPARE(html.count(QStringLiteral("<tr>")), 2);
    }

    // 标题、列表、粗体这些基础标记别在改表格时被弄坏
    void basicFormattingStillWorks()
    {
        const QString md = QStringLiteral("# 标题\n"
                                          "- 第一条\n"
                                          "- 第二条 **重点**\n");
        const QString html = MarkdownLite::toHtml(md);

        QVERIFY(html.contains(QStringLiteral("font-size:15px")));
        QCOMPARE(html.count(QStringLiteral("• ")), 2);
        QVERIFY(html.contains(QStringLiteral("<b>重点</b>")));
    }
};

QTEST_GUILESS_MAIN(MarkdownLiteTest)
#include "markdownlite_test.moc"
