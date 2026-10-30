#pragma once

#include <QDateTime>
#include <QString>

// M15-3 时间戳口径统一。
//
// 引擎一律输出带时区的 RFC3339 UTC（…Z，见引擎 internal/host/timefmt.go）；
// 老数据与兼容层可能给出 sqlite 的裸串 "2026-10-30 07:48:06" —— 那串本身
// 就是 UTC，只是没写时区。
//
// 所有时间显示都从这里出：会话列表、分组标题、记忆面板、消息时间。
// 以前各处直接 QDateTime::fromString(iso, Qt::ISODateWithMs)，裸串被当成
// **本地时间**解析，于是刚聊完的会话显示成 8 小时前，本地 00:00–08:00 之间
// 还会被分进「昨天」。
namespace FlareTime {

// 引擎时间串 → 本地时间。解析不了返回无效 QDateTime（调用方原样显示）。
QDateTime parse(const QString &iso);

// 列表条目：今天 HH:mm / 今年 MM-dd / 跨年 yyyy-MM-dd
QString listStamp(const QString &iso, const QDateTime &now = QDateTime());

// 分组标题：今天 / 昨天 / 最近 7 天 / 更早
QString section(const QString &iso, const QDateTime &now = QDateTime());

// 消息时间：今天 HH:mm / 昨天 HH:mm / 今年 MM-dd HH:mm / 跨年 yyyy-MM-dd HH:mm
QString messageStamp(const QString &iso, const QDateTime &now = QDateTime());
QString messageStampAt(const QDateTime &when, const QDateTime &now = QDateTime());

// 需要完整日期的场合（记忆面板）：yyyy-MM-dd HH:mm
QString fullStamp(const QString &iso);

} // namespace FlareTime
