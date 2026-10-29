import SwiftUI
import AppKit

// Theme.swift —— 视觉令牌唯一来源，对应 native/protocol/render-spec.md §1
// 铁律：不得在此之外自造色值。改样式先改 render-spec，再改本文件。

extension Color {
    init(hex: UInt32) {
        self.init(
            .sRGB,
            red: Double((hex >> 16) & 0xff) / 255.0,
            green: Double((hex >> 8) & 0xff) / 255.0,
            blue: Double(hex & 0xff) / 255.0,
            opacity: 1.0
        )
    }
}

enum Theme {
    // 火焰色阶
    static let red = Color(hex: 0xef4444)
    static let orange = Color(hex: 0xf97316)
    static let amber = Color(hex: 0xf59e0b)
    static let yellow = Color(hex: 0xfbbf24)
    static let dark = Color(hex: 0xb45309)

    // 文字
    static let textMain = Color(hex: 0x4a2e0d)
    static let textSoft = Color(hex: 0x8a6a3b)
    static let textMuted = Color(hex: 0x9a8f80)
    static let textGray = Color(hex: 0x4b5563)
    static let textFaint = Color(hex: 0xb9ab99)

    // 背景与描边
    static let bgWindow = Color(hex: 0xffffff)
    static let bgSoft = Color(hex: 0xfffbf0)
    static let bgAlt = Color(hex: 0xfff3d6)
    static let hoverSoft = Color(hex: 0xffedd0)
    static let borderSoft = Color(hex: 0xffe0b2)
    static let dangerBorder = Color(hex: 0xfda4a4)
    static let dangerText = Color(hex: 0xdc2626)

    // 字号阶梯（Qt 侧实测）
    enum FS {
        static let faint: CGFloat = 11
        static let small: CGFloat = 12
        static let base: CGFloat = 13
        static let body: CGFloat = 14
        static let title: CGFloat = 15
        static let chatTitle: CGFloat = 16
        static let big: CGFloat = 18
        static let hero: CGFloat = 22
    }

    /// 火焰渐变取色：t ∈ [0,1]，0 = red，0.5 = orange，1 = yellow（分段线性，同 Qt flameColor）
    static func flame(_ t: Double) -> Color {
        let x = min(max(t, 0), 1)
        if x < 0.5 {
            return lerp(red, orange, x / 0.5)
        }
        return lerp(orange, yellow, (x - 0.5) / 0.5)
    }

    /// 呼吸相位：5 秒一周期，幅度 0.12（同 Qt breathColor）
    static func breath(_ elapsed: Double) -> Double {
        1.0 - 0.12 * (0.5 - 0.5 * cos(elapsed * 2 * .pi / 5.0))
    }

    private static func lerp(_ a: Color, _ b: Color, _ t: Double) -> Color {
        let ca = NSColor(a).usingColorSpace(.sRGB) ?? .orange
        let cb = NSColor(b).usingColorSpace(.sRGB) ?? .yellow
        let k = min(max(t, 0), 1)
        return Color(
            .sRGB,
            red: Double(ca.redComponent) * (1 - k) + Double(cb.redComponent) * k,
            green: Double(ca.greenComponent) * (1 - k) + Double(cb.greenComponent) * k,
            blue: Double(ca.blueComponent) * (1 - k) + Double(cb.blueComponent) * k,
            opacity: 1.0
        )
    }
}

/// 逐字符火焰渐变文字（同 Qt gradientHtml）
struct FlameText: View {
    let text: String
    var size: CGFloat = Theme.FS.hero
    var weight: Font.Weight = .bold
    var phase: Double = 0

    var body: some View {
        let chars = Array(text)
        let n = max(chars.count - 1, 1)
        return HStack(spacing: size * 0.22) {
            ForEach(Array(chars.enumerated()), id: \.offset) { idx, ch in
                Text(String(ch))
                    .font(.system(size: size, weight: weight))
                    .foregroundColor(Theme.flame(Double(idx) / Double(n)))
            }
        }
        .brightness((Theme.breath(phase) - 1.0) * 0.9)
    }
}
