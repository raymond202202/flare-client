using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Effects;

namespace FlareWin
{
    /// <summary>
    /// 品牌一致的轻量弹窗。
    ///
    /// 为什么不用 MessageBox：系统弹窗是灰白 + 系统字体 + 系统按钮顺序，从浅橙界面里
    /// 弹出来像另一款软件的窗口；重命名还是高频动作。这里两个入口都走同一副骨架
    /// （白底、1px 暖描边、14px 圆角、暖色投影、右对齐按钮站），与主窗口同源。
    ///
    /// 用户偏好铁律：不许用系统 prompt()。所以重命名是正经模态框，不是输入行。
    /// </summary>
    internal static class Dialog
    {
        /// <summary>单行文本输入。返回 null 表示用户取消（Esc / 取消 / 关窗）。</summary>
        public static string Prompt(Window owner, string title, string initial, string okLabel = "保存")
        {
            var frame = Frame(title, 420, owner);
            var win = frame.Item1;
            var panel = frame.Item2;

            var box = new TextBox
            {
                Text = initial ?? "",
                FontSize = Theme.FontList,
                Foreground = Theme.TextMain,
                Background = Theme.BgSoft,
                BorderBrush = Theme.BorderSoft,
                BorderThickness = new Thickness(1),
                Padding = new Thickness(10, 8, 10, 8),
                Margin = new Thickness(0, 12, 0, 0),
                CaretBrush = Theme.Orange,
            };
            panel.Children.Add(box);

            var result = (string)null;
            var cancel = Ghost("取消");
            var ok = Primary(okLabel);
            cancel.Click += (o, e) => win.Close();
            // 回车确认、Esc 取消 —— 单行输入框的默认期待，别让人去够鼠标
            ok.Click += (o, e) => { result = box.Text; win.Close(); };
            panel.Children.Add(ButtonRow(cancel, ok));

            win.Loaded += (o, e) => { box.Focus(); box.SelectAll(); };
            win.PreviewKeyDown += (o, e) =>
            {
                if (e.Key == Key.Escape) { result = null; win.Close(); }
                else if (e.Key == Key.Enter) { result = box.Text; win.Close(); }
            };
            win.ShowDialog();
            return result;
        }

        /// <summary>二次确认。danger=true 时确认按钮换成红底（不可恢复的动作）。</summary>
        public static bool Confirm(Window owner, string title, string body, string okLabel = "确定", bool danger = false)
        {
            var frame = Frame(title, 440, owner);
            var win = frame.Item1;
            var panel = frame.Item2;

            panel.Children.Add(new TextBlock
            {
                Text = body,
                FontSize = Theme.FontList,
                Foreground = Theme.TextSoft,
                TextWrapping = TextWrapping.Wrap,
                Margin = new Thickness(0, 10, 0, 0),
            });

            var yes = false;
            var cancel = Ghost("取消");
            var ok = danger ? Danger(okLabel) : Primary(okLabel);
            cancel.Click += (o, e) => win.Close();
            ok.Click += (o, e) => { yes = true; win.Close(); };
            panel.Children.Add(ButtonRow(cancel, ok));

            win.Loaded += (o, e) => ok.Focus();
            win.PreviewKeyDown += (o, e) =>
            {
                if (e.Key == Key.Escape) { yes = false; win.Close(); }
                else if (e.Key == Key.Enter) { yes = true; win.Close(); }
            };
            win.ShowDialog();
            return yes;
        }

        // MARK: - 骨架

        private static System.Tuple<Window, StackPanel> Frame(string title, double width, Window owner)
        {
            var root = new Border
            {
                Background = Theme.BgWindow,
                BorderBrush = Theme.BorderSoft,
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(14),
                // 留出血位给投影；不留的话 Popup 边界会把阴影切掉一半，看着像脏了
                Margin = new Thickness(18),
            };
            root.Effect = new DropShadowEffect
            {
                BlurRadius = 24,
                ShadowDepth = 4,
                Direction = 270,
                Opacity = 0.20,
                Color = Theme.TextMainC,
            };

            var panel = new StackPanel { Margin = new Thickness(22, 18, 22, 18) };
            panel.Children.Add(new TextBlock
            {
                Text = title,
                FontSize = 15,
                FontWeight = FontWeights.SemiBold,
                Foreground = Theme.TextMain,
            });
            root.Child = panel;

            var win = new Window
            {
                Owner = owner,
                WindowStyle = WindowStyle.None,
                AllowsTransparency = true,
                Background = Brushes.Transparent,
                SizeToContent = SizeToContent.Height,
                Width = width,
                ResizeMode = ResizeMode.NoResize,
                WindowStartupLocation = WindowStartupLocation.CenterOwner,
                ShowInTaskbar = false,
                Content = root,
            };
            // 无边框窗口得自己给拖动，否则用户抓不住标题
            root.MouseLeftButtonDown += (o, e) =>
            {
                if (e.ButtonState == MouseButtonState.Pressed) win.DragMove();
            };
            return System.Tuple.Create(win, panel);
        }

        private static StackPanel ButtonRow(UIElement cancel, UIElement ok)
        {
            var sp = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                HorizontalAlignment = HorizontalAlignment.Right,
                Margin = new Thickness(0, 18, 0, 0),
            };
            cancel.SetValue(FrameworkElement.MarginProperty, new Thickness(0, 0, 8, 0));
            sp.Children.Add(cancel);
            sp.Children.Add(ok);
            return sp;
        }

        private static Button Ghost(string text) => new Button
        {
            Content = text,
            Style = (Style)Application.Current.FindResource("GhostBtn"),
            MinWidth = 84,
        };

        private static Button Primary(string text) => new Button
        {
            Content = text,
            Style = (Style)Application.Current.FindResource("PrimaryBtn"),
            MinWidth = 84,
            IsDefault = true,
        };

        private static Button Danger(string text) => new Button
        {
            Content = text,
            Style = (Style)Application.Current.FindResource("StopBtn"),
            MinWidth = 84,
            IsDefault = true,
        };
    }
}
