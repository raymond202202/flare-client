using System;
using System.Windows.Media;

namespace FlareWin
{
    /// <summary>
    /// 颜色令牌 —— 与 native/protocol/render-spec.md §1 **逐字节一致**。
    /// 任何平台不得自造色值；新增语义先改 render-spec 再改这里。
    /// </summary>
    public static class Theme
    {
        private static Color C(string hex)
        {
            var s = hex.TrimStart('#');
            return Color.FromRgb(
                Convert.ToByte(s.Substring(0, 2), 16),
                Convert.ToByte(s.Substring(2, 2), 16),
                Convert.ToByte(s.Substring(4, 2), 16));
        }

        private static SolidColorBrush B(Color c)
        {
            var b = new SolidColorBrush(c);
            b.Freeze();
            return b;
        }

        // §1.1 火焰色阶
        public static readonly Color RedC = C("#ef4444");
        public static readonly Color OrangeC = C("#f97316");
        public static readonly Color AmberC = C("#f59e0b");
        public static readonly Color YellowC = C("#fbbf24");
        public static readonly Color DarkC = C("#b45309");

        public static readonly SolidColorBrush Red = B(RedC);
        public static readonly SolidColorBrush Orange = B(OrangeC);
        public static readonly SolidColorBrush Amber = B(AmberC);
        public static readonly SolidColorBrush Yellow = B(YellowC);
        public static readonly SolidColorBrush Dark = B(DarkC);

        // §1.2 背景与描边
        public static readonly Color BgWindowC = C("#ffffff");
        public static readonly Color BgSoftC = C("#fffbf0");
        public static readonly Color BgAltC = C("#fff3d6");
        public static readonly Color BorderSoftC = C("#fde6bf");
        public static readonly Color BorderSoft2C = C("#fde3c6");
        public static readonly Color HoverSoftC = C("#ffedd0");

        public static readonly SolidColorBrush BgWindow = B(BgWindowC);
        public static readonly SolidColorBrush BgSoft = B(BgSoftC);
        public static readonly SolidColorBrush BgAlt = B(BgAltC);
        public static readonly SolidColorBrush BorderSoft = B(BorderSoftC);
        public static readonly SolidColorBrush BorderSoft2 = B(BorderSoft2C);
        public static readonly SolidColorBrush HoverSoft = B(HoverSoftC);

        // §1.3 文字
        public static readonly Color TextMainC = C("#4a2e0d");
        public static readonly Color TextSoftC = C("#8a6a3b");
        public static readonly Color TextMutedC = C("#9a8f80");
        public static readonly Color TextFaintC = C("#b9ab99");
        public static readonly Color TextGrayC = C("#4b5563");

        public static readonly SolidColorBrush TextMain = B(TextMainC);
        public static readonly SolidColorBrush TextSoft = B(TextSoftC);
        public static readonly SolidColorBrush TextMuted = B(TextMutedC);
        public static readonly SolidColorBrush TextFaint = B(TextFaintC);
        public static readonly SolidColorBrush TextGray = B(TextGrayC);

        // §1.4 交互态（主行动色派生）
        public static readonly SolidColorBrush BtnHover = B(C("#e0630f"));
        public static readonly SolidColorBrush BtnPressed = B(C("#c04f08"));
        public static readonly SolidColorBrush BtnDisabled = B(C("#e5e0d8"));
        public static readonly SolidColorBrush StopBorder = B(C("#fda4a4"));
        public static readonly SolidColorBrush StopHoverBg = B(C("#fef2f2"));
        public static readonly SolidColorBrush StopPressedBg = B(C("#fde8e8"));

        /// <summary>
        /// §1.5 火焰渐变：t 任意实数取小数部分，前 0.5 红→橙，后 0.5 橙→黄。
        /// 三端必须同算法，用于欢迎词逐字符着色与呼吸效果。
        /// </summary>
        public static Color FlameColor(double t)
        {
            var tt = t - Math.Floor(t);
            if (tt < 0)
                tt += 1.0; // 负数取模修正，保证 [0,1)
            byte r, g, b;
            if (tt < 0.5)
            {
                var u = tt * 2.0;
                r = (byte)Math.Round(0xef + (0xf9 - 0xef) * u);
                g = (byte)Math.Round(0x44 + (0x73 - 0x44) * u);
                b = (byte)Math.Round(0x44 + (0x16 - 0x44) * u);
            }
            else
            {
                var u = (tt - 0.5) * 2.0;
                r = (byte)Math.Round(0xf9 + (0xfb - 0xf9) * u);
                g = (byte)Math.Round(0x73 + (0xbf - 0x73) * u);
                b = (byte)Math.Round(0x16 + (0x24 - 0x16) * u);
            }
            return Color.FromRgb(r, g, b);
        }

        /// <summary>§1.5 呼吸相位：5s 周期，sin(t·4π) 映射到 [0,1]。</summary>
        public static Color BreathColor(double nowMs)
        {
            var t = (nowMs % 5000.0) / 5000.0;
            var phase = (Math.Sin(t * 4.0 * Math.PI) + 1.0) / 2.0;
            return FlameColor(phase);
        }

        /// <summary>§2 字号阶梯（绝对 px，跟随系统 DPI）。</summary>
        public const double FontFootnote = 11;
        public const double FontSmall = 12;
        public const double FontList = 13;
        public const double FontBody = 14;
        public const double FontPanelTitle = 15;
        public const double FontChatTitle = 16;
        public const double FontSettingsTitle = 18;

        public const string FontUi = "Segoe UI";
        public const string FontMono = "Consolas";
    }
}
