#ifndef MEDIATAGS_H
#define MEDIATAGS_H

#include <QJsonArray>
#include <QString>

// 引擎只下发「事实」：这条内容里有 image / file。
// 「[图片]」「[附件]」是给人看的词，属于展示层 —— 统一在这里翻译，
// 会话列表和历史消息共用同一套口径，不各写一份。
namespace MediaTags {

// 事实 → 标签串（如 "[图片] [附件]"）；没有非文本成分时返回空串。
QString label(const QJsonArray &kinds);

// 标签前置到正文；正文为空时只返回标签。两者都空则返回空串。
QString withText(const QString &text, const QJsonArray &kinds);

} // namespace MediaTags

#endif // MEDIATAGS_H
