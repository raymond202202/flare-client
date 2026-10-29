using System;
using System.Collections.Specialized;
using System.ComponentModel;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace FlareWin
{
    public partial class MainWindow : Window
    {
        private readonly Engine _engine = new Engine();
        private DispatcherTimer _breathTimer;
        private double _phase;

        public MainWindow()
        {
            InitializeComponent();
            DataContext = _engine;

            _engine.PropertyChanged += OnEnginePropertyChanged;
            _engine.Blocks.CollectionChanged += OnBlocksChanged;

            Loaded += (s, e) =>
            {
                SideVersion.Text = "v" + _engine.ClientVersion;
                ApplyFlameWelcome();
                _engine.Start();
                StartSmokeHooks();
            };
            Closed += (s, e) =>
            {
                _breathTimer?.Stop();
                _engine.Stop();
            };
        }

        // ================= 引擎状态 → 界面 =================

        private void OnEnginePropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            switch (e.PropertyName)
            {
                case nameof(Engine.StatusText):
                    StatusLabel.Text = _engine.StatusText;
                    break;
                case nameof(Engine.Status):
                    StatusDot.Background = _engine.Status == Engine.EngineStatus.Ready
                        ? Theme.Yellow
                        : Theme.TextFaint;
                    break;
                case nameof(Engine.ModelName):
                case nameof(Engine.ProviderName):
                case nameof(Engine.EngineVersion):
                    UpdateModelBadge();
                    break;
                case nameof(Engine.UsageText):
                    UpdateRightStatus();
                    break;
                case nameof(Engine.Busy):
                    SyncSendStop();
                    break;
                case nameof(Engine.PendingConfirm):
                    ShowConfirmIfAny();
                    break;
                case nameof(Engine.SessionID):
                    UpdateChatTitle();
                    break;
            }
        }

        private void UpdateModelBadge()
        {
            var parts = "";
            if (!string.IsNullOrEmpty(_engine.ModelName)) parts += _engine.ModelName;
            if (!string.IsNullOrEmpty(_engine.ProviderName))
                parts += (parts.Length > 0 ? " · " : "") + _engine.ProviderName;
            if (parts.Length == 0) parts = "—";
            ModelBadge.Text = parts;
            UpdateRightStatus();
        }

        private void UpdateRightStatus()
        {
            var v = "引擎 v" + _engine.EngineVersion;
            if (!string.IsNullOrEmpty(_engine.UsageText)) v += "   ·   " + _engine.UsageText;
            RightStatus.Text = v;
        }

        private void SyncSendStop()
        {
            var busy = _engine.Busy;
            SendButton.Visibility = busy ? Visibility.Collapsed : Visibility.Visible;
            StopButton.Visibility = busy ? Visibility.Visible : Visibility.Collapsed;
            InputShell.Opacity = 1.0;
            HintLine.Text = busy ? "Flare 正在生成…点「停止」可中断" : "";
        }

        private void UpdateChatTitle()
        {
            ChatTitle.Text = "新对话";
            foreach (var s in _engine.Sessions)
            {
                if (s.Id == _engine.SessionID)
                {
                    ChatTitle.Text = s.Display;
                    break;
                }
            }
        }

        // ================= 聊天区 =================

        private void OnBlocksChanged(object sender, NotifyCollectionChangedEventArgs e)
        {
            // 集合变化后一次性的收尾：滚到底 + 欢迎页互斥
            Dispatcher.BeginInvoke(new Action(() =>
            {
                ChatScroll.ScrollToEnd();
                Welcome.Visibility = _engine.Blocks.Count == 0
                    ? Visibility.Visible
                    : Visibility.Collapsed;
            }), DispatcherPriority.Background);
        }

        /// <summary>欢迎语逐字火焰渐变（render-spec §1.5 + §5），5s 呼吸一次。</summary>
        private void ApplyFlameWelcome()
        {
            RenderFlame(0);
            _breathTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(80) };
            _breathTimer.Tick += (s, e) =>
            {
                _phase += 0.08;
                RenderFlame(_phase);
            };
            _breathTimer.Start();
        }

        private void RenderFlame(double t)
        {
            var text = "有什么可以帮你的？";
            WelcomeLine1.Inlines.Clear();
            for (var i = 0; i < text.Length; i++)
            {
                var c = Theme.FlameColor(t + i * 0.06);
                WelcomeLine1.Inlines.Add(new Run(text[i].ToString())
                {
                    Foreground = new SolidColorBrush(c)
                });
            }
        }

        // ================= 输入 =================

        private void OnInputTextChanged(object sender, TextChangedEventArgs e)
        {
            InputPlaceholder.Visibility = string.IsNullOrEmpty(InputBox.Text)
                ? Visibility.Visible
                : Visibility.Collapsed;
        }

        private void OnInputKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter && (Keyboard.Modifiers & ModifierKeys.Shift) == 0)
            {
                e.Handled = true;
                DoSend();
            }
        }

        private void OnSend(object sender, RoutedEventArgs e) => DoSend();

        private void DoSend()
        {
            var t = InputBox.Text;
            if (string.IsNullOrWhiteSpace(t)) return;
            if (_engine.Busy) return;
            _engine.SendChat(t);
            InputBox.Clear();
            InputBox.Focus();
        }

        private void OnStop(object sender, RoutedEventArgs e) => _engine.Cancel();

        // ================= 侧栏 =================

        private void OnNewChat(object sender, RoutedEventArgs e)
        {
            _engine.NewSession();
            _engine.Blocks.Clear();
            ChatTitle.Text = "新对话";
            SessionList.SelectedItem = null;
        }

        private void OnSessionSelected(object sender, SelectionChangedEventArgs e)
        {
            if (SessionList.SelectedItem is SessionInfo s)
            {
                _engine.SwitchSession(s.Id);
                ChatTitle.Text = s.Display;
            }
        }

        private void OnArchived(object sender, RoutedEventArgs e)
        {
            // 归档列表复用同一个面板（只读浏览 + 点击载入）
            var w = new ListPanelWindow("归档会话", "archived")
            {
                Owner = this
            };
            w.BindSessions(_engine.ArchivedSessions);
            _engine.LoadArchived();
            w.ShowDialog();
        }

        private void OnMemories(object sender, RoutedEventArgs e)
        {
            var w = new ListPanelWindow("记忆", "memories") { Owner = this };
            w.BindMemories(_engine.Memories, _engine);
            _engine.LoadMemories();
            w.ShowDialog();
        }

        private void OnTools(object sender, RoutedEventArgs e)
        {
            var w = new ListPanelWindow("技能", "tools") { Owner = this };
            w.BindTools(_engine.Tools);
            _engine.LoadTools();
            w.ShowDialog();
        }

        private void OnUsage(object sender, RoutedEventArgs e)
        {
            _engine.LoadUsage();
            MessageBox.Show(this,
                string.IsNullOrEmpty(_engine.UsageText) ? "用量数据获取中，稍后点一次再看。" : _engine.UsageText,
                "用量", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        // ================= 工具确认 =================

        private void ShowConfirmIfAny()
        {
            var c = _engine.PendingConfirm;
            if (c == null) return;
            var r = MessageBox.Show(this,
                "Flare 想执行工具：" + c.Tool + "\n\n" + c.Detail + "\n\n允许本次调用吗？",
                "需要确认", MessageBoxButton.YesNo, MessageBoxImage.Question);
            _engine.AnswerConfirm(r == MessageBoxResult.Yes ? "allow" : "deny");
        }

        // ================= 自测钩子（CI / 无人工环境） =================

        private void StartSmokeHooks()
        {
            var shotDir = Environment.GetEnvironmentVariable("FLARE_GUI_SHOT");
            if (string.IsNullOrEmpty(shotDir))
            {
                if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable("FLARE_GUI_SMOKE"))) return;
            }

            // 启动 3.5s 后：01-main
            Delay(3500, () => Shot("01-main"));

            var smoke = Environment.GetEnvironmentVariable("FLARE_GUI_SMOKE");
            if (string.IsNullOrEmpty(smoke)) return;

            // 引擎开始流式输出后 1.5s：06-busy
            var busyArmed = false;
            var poll = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(300) };
            poll.Tick += (s, e) =>
            {
                if (!busyArmed && _engine.Busy)
                {
                    busyArmed = true;
                    Delay(1500, () => Shot("06-busy"));
                }
                if (_engine.CancelledOnce)
                {
                    poll.Stop();
                    Delay(1500, () => Shot("07-cancelled"));
                }
            };
            poll.Start();
        }

        private static void Delay(int ms, Action a)
        {
            var t = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(ms) };
            t.Tick += (s, e) =>
            {
                t.Stop();
                a();
            };
            t.Start();
        }

        /// <summary>
        /// WPF 应用内自截图：RenderTargetBitmap 直接渲染可视树，
        /// 不需要屏幕录制权限、不受远程桌面/最小化影响（与 macOS 的 ShotKit 同理）。
        /// </summary>
        private void Shot(string name)
        {
            var dir = Environment.GetEnvironmentVariable("FLARE_GUI_SHOT");
            if (string.IsNullOrEmpty(dir)) return;
            try
            {
                Directory.CreateDirectory(dir);
                var root = (FrameworkElement)Content;
                root.UpdateLayout();
                var w = (int)Math.Ceiling(root.ActualWidth);
                var h = (int)Math.Ceiling(root.ActualHeight);
                if (w <= 0 || h <= 0) return;
                var rtb = new RenderTargetBitmap(w, h, 96, 96, PixelFormats.Pbgra32);
                rtb.Render(root);
                var enc = new PngBitmapEncoder();
                enc.Frames.Add(BitmapFrame.Create(rtb));
                var p = Path.Combine(dir, name + ".png");
                using (var fs = File.Create(p)) enc.Save(fs);
                LogShot("shot " + p + " " + w + "x" + h);
            }
            catch (Exception ex)
            {
                LogShot("shot failed: " + ex.Message);
            }
        }

        private static void LogShot(string s)
        {
            var p = Environment.GetEnvironmentVariable("FLARE_GUI_LOG");
            if (string.IsNullOrEmpty(p)) return;
            try
            {
                File.AppendAllText(p, DateTime.Now.ToString("HH:mm:ss.fff") + " " + s + "\n");
            }
            catch
            {
            }
        }
    }
}
