// M15-3 时间戳口径：所有时间显示都必须经过 FlareTime。
//
// 曾经的事故：数据库用 sqlite 的 datetime('now') 存时间，那是 UTC 但字符串里
// 没有时区标记。各面板直接 QDateTime::fromString(iso, Qt::ISODateWithMs)，
// Qt 把裸串当**本地时间**，于是刚聊完的会话显示成 8 小时前，本地 00:00–08:00
// 之间的会话还会被分进「昨天」那一组。
//
// 这里的断言刻意做成**与时区无关**：不硬编码 "15:00"，而是拿 UTC 时刻推本地
// 期望值，或者直接断言「裸串」与「带 Z 串」走的是同一个时刻。CI 跑在 UTC、
// 用户跑在 +08:00，结论必须一样。
#include <QtTest>
#include <QDateTime>
#include <QTimeZone>

#include "flaretime.h"

class FlareTimeTest : public QObject
{
    Q_OBJECT

private slots:
    void bareUtcAndZuluAreTheSameInstant()
    {
        const QDateTime bare = FlareTime::parse(QStringLiteral("2026-10-30 07:00:00"));
        const QDateTime zulu = FlareTime::parse(QStringLiteral("2026-10-30T07:00:00Z"));
        QVERIFY(bare.isValid());
        QVERIFY(zulu.isValid());
        QCOMPARE(bare.toUTC(), zulu.toUTC());
        QCOMPARE(bare.toSecsSinceEpoch(), zulu.toSecsSinceEpoch());
    }

    void bareUtcIsNotMistakenForLocal()
    {
        // 核心回归：裸串必须按 UTC 解释。若谁又改回 fromString(..., ISODate)，
        // 这里会在 +08:00 的机器上差 8 小时。
        const QDateTime bare = FlareTime::parse(QStringLiteral("2026-10-30 07:00:00"));
        const QDateTime utc(QDate(2026, 10, 30), QTime(7, 0), QTimeZone::UTC);
        QCOMPARE(bare.toSecsSinceEpoch(), utc.toSecsSinceEpoch());
        QCOMPARE(bare.toString(Qt::ISODate), utc.toLocalTime().toString(Qt::ISODate));
    }

    void offsetInSourceIsRespected()
    {
        const QDateTime plus8 = FlareTime::parse(QStringLiteral("2026-10-30T15:00:00+08:00"));
        const QDateTime zulu = FlareTime::parse(QStringLiteral("2026-10-30T07:00:00Z"));
        QCOMPARE(plus8.toUTC(), zulu.toUTC());
    }

    void millisecondsAndDateOnlyStillParse()
    {
        const QDateTime withMs = FlareTime::parse(QStringLiteral("2026-10-30 07:00:00.250"));
        const QDateTime plain = FlareTime::parse(QStringLiteral("2026-10-30 07:00:00"));
        QCOMPARE(withMs.toSecsSinceEpoch(), plain.toSecsSinceEpoch());

        QVERIFY(FlareTime::parse(QStringLiteral("2026-10-30")).isValid());
    }

    void garbageIsShownAsGarbageNeverAs1970()
    {
        // 解析不了就原样显示：宁可看到怪串，也不要把时间清成 1970 骗人。
        QCOMPARE(FlareTime::listStamp(QStringLiteral("不是时间")), QStringLiteral("不是时间"));
        QCOMPARE(FlareTime::fullStamp(QStringLiteral("不是时间")), QStringLiteral("不是时间"));
        QCOMPARE(FlareTime::section(QStringLiteral("不是时间")), QStringLiteral("更早"));
        QCOMPARE(FlareTime::messageStamp(QStringLiteral("不是时间")), QString());
        QVERIFY(!FlareTime::parse(QString()).isValid());
        QCOMPARE(FlareTime::listStamp(QString()), QString());
    }

    void sectionFollowsLocalCalendar()
    {
        // now 用本地时间构造，被测值用本地时刻构造 → 与时区无关
        const QDateTime now = QDateTime(QDate(2026, 10, 30), QTime(15, 0));
        const QString todayIso = now.addSecs(-3600).toUTC().toString(Qt::ISODate);
        const QString yesterdayIso = now.addDays(-1).toUTC().toString(Qt::ISODate);

        QCOMPARE(FlareTime::section(todayIso, now), QStringLiteral("今天"));
        QCOMPARE(FlareTime::section(yesterdayIso, now), QStringLiteral("昨天"));
        QCOMPARE(FlareTime::section(now.addDays(-3).toUTC().toString(Qt::ISODate), now),
                 QStringLiteral("最近 7 天"));
        QCOMPARE(FlareTime::section(now.addDays(-40).toUTC().toString(Qt::ISODate), now),
                 QStringLiteral("更早"));
    }

    void middleOfNightStaysInToday()
    {
        // 具体回归：本地 2026-10-30 07:30 的会话，DB 里写的是 UTC 2026-10-29 23:30。
        // 以前按本地解释就被分进「昨天」，明明刚聊完却跑到昨天那一组去了。
        const QDateTime now = QDateTime(QDate(2026, 10, 30), QTime(7, 40));
        const QDateTime earlyMorning = QDateTime(QDate(2026, 10, 30), QTime(7, 30));
        const QString iso = earlyMorning.toUTC().toString(Qt::ISODate);
        QCOMPARE(FlareTime::section(iso, now), QStringLiteral("今天"));
        QCOMPARE(FlareTime::listStamp(iso, now),
                 earlyMorning.toString(QStringLiteral("HH:mm")));
    }

    void listStampShrinksWithAge()
    {
        const QDateTime now = QDateTime(QDate(2026, 10, 30), QTime(15, 0));
        const QDateTime today = now.addSecs(-600);
        QCOMPARE(FlareTime::listStamp(today.toUTC().toString(Qt::ISODate), now),
                 today.toString(QStringLiteral("HH:mm")));

        const QDateTime spring = QDateTime(QDate(2026, 3, 15), QTime(9, 0));
        QCOMPARE(FlareTime::listStamp(spring.toUTC().toString(Qt::ISODate), now),
                 QStringLiteral("03-15"));

        const QDateTime old = QDateTime(QDate(2025, 3, 15), QTime(9, 0));
        QCOMPARE(FlareTime::listStamp(old.toUTC().toString(Qt::ISODate), now),
                 QStringLiteral("2025-03-15"));
    }

    void messageStampCarriesDayWhenNeeded()
    {
        const QDateTime now = QDateTime(QDate(2026, 10, 30), QTime(15, 0));
        const QDateTime justNow = now.addSecs(-120);
        QCOMPARE(FlareTime::messageStampAt(justNow, now), justNow.toString(QStringLiteral("HH:mm")));

        const QDateTime yesterday = now.addDays(-1);
        QCOMPARE(FlareTime::messageStampAt(yesterday, now),
                 QStringLiteral("昨天 ") + yesterday.toString(QStringLiteral("HH:mm")));

        const QDateTime spring = QDateTime(QDate(2026, 3, 15), QTime(9, 7));
        QCOMPARE(FlareTime::messageStampAt(spring, now), QStringLiteral("03-15 09:07"));

        const QDateTime old = QDateTime(QDate(2024, 3, 15), QTime(9, 7));
        QCOMPARE(FlareTime::messageStampAt(old, now), QStringLiteral("2024-03-15 09:07"));

        QCOMPARE(FlareTime::messageStampAt(QDateTime(), now), QString());
    }

    void fullStampKeepsFullDate()
    {
        const QDateTime when = QDateTime(QDate(2026, 10, 30), QTime(7, 5));
        QCOMPARE(FlareTime::fullStamp(when.toUTC().toString(Qt::ISODate)),
                 QStringLiteral("2026-10-30 07:05"));
    }
};

QTEST_MAIN(FlareTimeTest)
#include "flaretime_test.moc"
