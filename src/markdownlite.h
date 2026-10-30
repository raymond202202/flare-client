#ifndef MARKDOWNLITE_H
#define MARKDOWNLITE_H

#include <QString>

// ============================================================
// MarkdownLite —— 助手回复的轻量 Markdown → Qt 富文本 HTML 片段
//
// 为什么需要：引擎回复里带 **粗体**、`行内代码`、```代码块```、- 列表，
// 之前 QTextEdit 用纯文本追加，用户看到的是原样星号和反引号。
//
// 设计约束：
//   - 输出**内联片段**（不用 <p> 包裹），这样才能用 QTextCursor::insertHtml
//     直接替换流式文本区间，不打断「Flare：」标签所在的块。
//   - 只支持高频语法，宁可漏渲染也不要吃掉正文。
// ============================================================
namespace MarkdownLite {

QString toHtml(const QString &text);

} // namespace MarkdownLite

#endif // MARKDOWNLITE_H
