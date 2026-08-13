#ifndef FLAMETHEME_H
#define FLAMETHEME_H

#include <QColor>
#include <QString>
#include <cmath>
// ============================================================
// 火焰主题（M5-1）—— 移植自 flare 命令行版视觉规范
// 来源: ~/hermes-projects/flare/src/cli/flame-banner.ts
//   FLAME_TOKENS: red #ef4444 / orange #f97316 / amber #f59e0b
//                 / yellow #fbbf24 / dark #b45309
//   flameColor(t): 红→橙→黄插值（t<0.5 红→橙，否则橙→黄）
// 多端统一的火焰视觉令牌（CLI / 桌面版共用）
// ============================================================
namespace FlameTheme {

// ---- 色阶 ----
inline QColor red()    { return QColor(0xef, 0x44, 0x44); } // 火焰核心
inline QColor orange() { return QColor(0xf9, 0x73, 0x16); } // 主行动色
inline QColor amber()  { return QColor(0xf5, 0x9e, 0x0b); } // 过渡色
inline QColor yellow() { return QColor(0xfb, 0xbf, 0x24); } // 亮黄
inline QColor dark()   { return QColor(0xb4, 0x53, 0x09); } // 暗琥珀

// ---- 浅色背景系（火焰主题的柔和底色，替代原紫配浅底）----
inline QColor bgSoft()    { return QColor(0xff, 0xfb, 0xf0); } // 米白底
inline QColor bgAlt()     { return QColor(0xff, 0xf3, 0xd6); } // 浅琥珀底
inline QColor borderSoft(){ return QColor(0xfd, 0xe6, 0xbf); } // 浅橙边框
inline QColor hoverSoft() { return QColor(0xff, 0xed, 0xd0); } // 悬停底

// ---- 文字色 ----
inline QColor textMain()  { return QColor(0x4a, 0x2e, 0x0d); } // 深琥珀文字
inline QColor textSoft()  { return QColor(0x8a, 0x6a, 0x3b); } // 次级文字

// ---- 渐变算法（移植 flameColor）----
// t: 0~1，t<0.5 红→橙，t>=0.5 橙→黄；自动取模
inline QColor flameColor(double t)
{
    double tt = t - std::floor(t);
    double r, g, b;
    if (tt < 0.5) {
        double u = tt * 2;
        r = 0xef + (0xf9 - 0xef) * u;
        g = 0x44 + (0x73 - 0x44) * u;
        b = 0x44 + (0x16 - 0x44) * u;
    } else {
        double u = (tt - 0.5) * 2;
        r = 0xf9 + (0xfb - 0xf9) * u;
        g = 0x73 + (0xbf - 0x73) * u;
        b = 0x16 + (0x24 - 0x16) * u;
    }
    return QColor(static_cast<int>(std::lround(r)),
                  static_cast<int>(std::lround(g)),
                  static_cast<int>(std::lround(b)));
}

// 呼吸色（flameBreathColor）：时间驱动的渐变相位
inline QColor breathColor(qint64 timeMs)
{
    double t = (timeMs % 5000) / 5000.0;
    double phase = (std::sin(t * M_PI * 4) + 1) / 2;
    return flameColor(phase);
}

// 逐字符火焰渐变：返回 HTML 富文本（每字符独立颜色）
// text 中空格/emoji 不着色；reverse 让句尾落在红色端
inline QString gradientHtml(const QString &text, bool reverse = false)
{
    QString html;
    const auto chars = text.toUcs4();
    const int len = chars.size();
    for (int i = 0; i < len; ++i) {
        const char32_t cp = static_cast<char32_t>(chars[i]);
        if (cp == 0x20 || cp > 0xffff) {
            html += QString::fromUcs4(reinterpret_cast<const char32_t *>(&cp), 1);
            continue;
        }
        double t = len <= 1 ? 0 : double(i) / (len - 1);
        if (reverse) t = 1 - t;
        const QColor c = flameColor(t);
        html += QStringLiteral("<span style=\"color:%1;\">%2</span>")
                    .arg(c.name(),
                         QString::fromUcs4(reinterpret_cast<const char32_t *>(&cp), 1).toHtmlEscaped());
    }
    return html;
}

// 欢迎词（M5-2）：与命令行版同款
inline QString welcomeText()
{
    return QStringLiteral("F L A R E\nLet your inspiration flare 🔥");
}

} // namespace FlameTheme

#endif // FLAMETHEME_H
