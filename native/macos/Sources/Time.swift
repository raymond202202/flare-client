import Foundation

// Time.swift —— 逐字复刻 Qt 版 src/flaretime.cpp（基线 0.6.191）
// 三端时间口径必须是同一份规则：分组标题、列表时间列、tooltip 完整时间全部从这里出。
// 引擎给的是 RFC3339 字符串（带 Z 或 +08:00，可能带毫秒），一律按**本地时区**解释，
// 避免凌晨会话被分进「昨天」。
enum FlareTime {

    /// 引擎时间串 → Date。解析不了返回 nil（调用方原样显示或留空）。
    static func parse(_ iso: String) -> Date? {
        let s = iso.trimmingCharacters(in: .whitespacesAndNewlines)
        if s.isEmpty { return nil }

        let f1 = ISO8601DateFormatter()
        f1.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        if let d = f1.date(from: s) { return d }

        let f2 = ISO8601DateFormatter()
        f2.formatOptions = [.withInternetDateTime]
        if let d = f2.date(from: s) { return d }

        // 引擎少数路径给不带时区的本地时间串
        let df = DateFormatter()
        df.locale = Locale(identifier: "en_US_POSIX")
        df.timeZone = TimeZone.current
        for fmt in ["yyyy-MM-dd'T'HH:mm:ss", "yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd"] {
            df.dateFormat = fmt
            if let d = df.date(from: s) { return d }
        }
        return nil
    }

    /// 列表条目右侧的时间列：今天 HH:mm / 今年 MM-dd / 跨年 yy-MM-dd
    /// 侧栏窄，完整时间会吃掉近一半宽度，精确时间留给 tooltip。
    static func listStampCompact(_ iso: String, now: Date = Date()) -> String {
        guard let d = parse(iso) else { return "" }
        let cal = Calendar.current
        let df = DateFormatter()
        df.locale = Locale(identifier: "en_US_POSIX")
        if cal.isDate(d, inSameDayAs: now) {
            df.dateFormat = "HH:mm"
        } else if cal.component(.year, from: d) == cal.component(.year, from: now) {
            df.dateFormat = "MM-dd"
        } else {
            df.dateFormat = "yy-MM-dd"
        }
        return df.string(from: d)
    }

    /// tooltip 用的完整时间：今天 HH:mm / 今年 MM-dd HH:mm / 跨年 yyyy-MM-dd HH:mm
    static func fullStamp(_ iso: String, now: Date = Date()) -> String {
        guard let d = parse(iso) else { return "" }
        let cal = Calendar.current
        let df = DateFormatter()
        df.locale = Locale(identifier: "en_US_POSIX")
        if cal.isDate(d, inSameDayAs: now) {
            df.dateFormat = "HH:mm"
        } else if cal.component(.year, from: d) == cal.component(.year, from: now) {
            df.dateFormat = "MM-dd HH:mm"
        } else {
            df.dateFormat = "yyyy-MM-dd HH:mm"
        }
        return df.string(from: d)
    }

    /// 分组标题：今天 / 昨天 / 最近 7 天 / 更早（与 flaretime.cpp 完全一致）
    static func section(_ iso: String, now: Date = Date()) -> String {
        guard let d = parse(iso) else { return "更早" }
        let cal = Calendar.current
        if cal.isDate(d, inSameDayAs: now) { return "今天" }
        if let y = cal.date(byAdding: .day, value: -1, to: now), cal.isDate(d, inSameDayAs: y) {
            return "昨天"
        }
        let today = cal.startOfDay(for: now)
        if let seven = cal.date(byAdding: .day, value: -6, to: today), d >= seven {
            return "最近 7 天"
        }
        return "更早"
    }

    /// 引擎建库默认 title='新会话'，这类占位标题用户认不出会话在干什么，
    /// 宿主侧要用首条消息 preview 顶上（Qt M8 口径）。
    static func isPlaceholderTitle(_ title: String) -> Bool {
        let t = title.trimmingCharacters(in: .whitespacesAndNewlines)
        return t.isEmpty
            || t == "新会话"
            || t == "default"
            || t == "untitled"
            || t.hasPrefix("gui-")
    }
}
