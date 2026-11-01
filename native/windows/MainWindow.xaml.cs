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
        private DispatcherTimer _searchTimer;
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
                // 标题行 Id 是空串，新会话时 SessionID 也可能为空，不排掉会给标题行配上名字
                if (s.IsHeader) continue;
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

        // ================= 搜索 =================

        /// <summary>
        /// 搜索框输入：250ms 防抖后问引擎（Qt sessionlistwidget.cpp:296-298 同一条）。
        /// 清空时**不等防抖**——用户点了叉还要愣 250ms 才看到列表回来，像卡住了。
        /// </summary>
        private void OnSearchTextChanged(object sender, TextChangedEventArgs e)
        {
            SearchPlaceholder.Visibility = SearchBox.Text.Length == 0
                ? Visibility.Visible
                : Visibility.Collapsed;

            // 立刻同步给引擎：它拿这个判「分组标题插不插」。
            // 搜索激活时还按今天/昨天/更早分组只是噪音（Qt :604 的注释原话）。
            _engine.SearchQuery = SearchBox.Text;

            if (SearchBox.Text.Trim().Length == 0)
            {
                StopSearchTimer();
                _engine.ClearSearch();
                return;
            }

            if (_searchTimer == null)
            {
                _searchTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(250) };
                _searchTimer.Tick += (s2, e2) =>
                {
                    StopSearchTimer();
                    _engine.SearchSessions(SearchBox.Text);
                };
            }
            // 攒一下再发：中文输入法的拼音过程、连续退格都不该每帧打一次全表 LIKE
            _searchTimer.Stop();
            _searchTimer.Start();
        }

        private void StopSearchTimer()
        {
            if (_searchTimer != null) _searchTimer.Stop();
        }

        /// <summary>搜索框里敲 ↓ 直接进结果列表（Qt :290 同一条）；Esc 清空回到原视图。</summary>
        private void OnSearchKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Down)
            {
                e.Handled = true;
                var idx = FirstSelectableIndex();
                if (idx >= 0) SessionList.SelectedIndex = idx;
                return;
            }
            if (e.Key == Key.Escape && SearchBox.Text.Length > 0)
            {
                e.Handled = true;
                SearchBox.Text = "";
            }
        }

        /// <summary>首个能选中的行。分组标题行的 Id 是空串且 IsHeader，跳过它。</summary>
        private int FirstSelectableIndex()
        {
            for (var i = 0; i < _engine.Sessions.Count; i++)
                if (!_engine.Sessions[i].IsHeader) return i;
            return -1;
        }

        /// <summary>Ctrl+F 聚焦搜索框（Qt mainwindow.cpp 同一条快捷键）。</summary>
        protected override void OnPreviewKeyDown(KeyEventArgs e)
        {
            if (!e.Handled && e.Key == Key.F && (Keyboard.Modifiers & ModifierKeys.Control) != 0)
            {
                e.Handled = true;
                SearchBox.Focus();
                SearchBox.SelectAll();
                return;
            }
            base.OnPreviewKeyDown(e);
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
                // 分组标题行没有 id，掉进 SwitchSession 会拿空串去查引擎
                if (s.IsHeader) return;
                ChatTitle.Text = s.Display;
                // 截图样本不是真会话，去引擎 load_session 只会拿回一条 error 糊在画面上
                if (s.Id.StartsWith("demo-")) return;
                _engine.SwitchSession(s.Id);
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

            // 选中态不在启动时挂定时器：侧栏每次刷新都会 Clear + 重建集合，
            // 3.0s 选中的项到 3.5s 拍照时可能已经被刷掉，于是截出来没有高亮。
            // 统一挪到 Shot() 里，在渲染前一刻补选，见 EnsureDemoSelection()。

            // 启动 3.5s 后：01-main
            Delay(3500, () => Shot("01-main"));

            // FLARE_GUI_SEARCH=Go → 01-main 拍完再灌关键词，等防抖 + 回执 + 重排都落定
            // 之后补一张 02-search。这张专门用来验两件事：搜索态下**没有**分组标题，
            // 置顶项仍在最前且带 📌 前缀。
            var searchKw = Environment.GetEnvironmentVariable("FLARE_GUI_SEARCH");
            if (!string.IsNullOrEmpty(searchKw))
            {
                Delay(4200, () =>
                {
                    LogShot("search \"" + searchKw + "\"");
                    SearchBox.Text = searchKw;   // → TextChanged → 250ms 防抖 → search_sessions
                    Delay(1500, () => Shot("02-search"));
                });
            }

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

                // 拍摄前一瞬补选中：选中态是侧栏最容易写错的一处
                // （Foreground 写成元素属性会被本地值优先级压过 DataTrigger，选中后字不变白），
                // 拍不到就等于这一轮没验。
                EnsureDemoSelection();

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

        /// <summary>
        /// 取证专用：保证侧栏有一条真会话处于选中态。分组标题行是 IsEnabled=False，
        /// 跳过它 —— 否则 SelectedIndex=0 落在标题行上会被 WPF 静默忽略。
        /// </summary>
        private void EnsureDemoSelection()
        {
            if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable("FLARE_GUI_DEMO"))) return;
            var cur = SessionList.SelectedItem as SessionInfo;
            if (cur != null && !cur.IsHeader) return;
            for (int i = 0; i < SessionList.Items.Count; i++)
            {
                var it = SessionList.Items[i] as SessionInfo;
                if (it != null && !it.IsHeader) { SessionList.SelectedIndex = i; break; }
            }
            // 选中会联动标题栏，必须让这次变更落进布局再截
            SessionList.UpdateLayout();
            var picked = SessionList.SelectedItem as SessionInfo;
            LogShot("选中 idx=" + SessionList.SelectedIndex + " id=" + (picked == null ? "<null>" : picked.Id));
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
