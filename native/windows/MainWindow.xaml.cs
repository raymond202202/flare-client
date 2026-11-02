using System;
using System.Collections.Specialized;
using System.ComponentModel;
using System.IO;
using System.Text;
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

        private void OnArchived(object sender, RoutedEventArgs e) => OpenArchivePanel();

        /// <summary>归档面板（回收站）：底部「归档」按钮与右键菜单末项共用。</summary>
        private void OpenArchivePanel()
        {
            // 归档列表复用同一个面板（只读浏览 + 点击载入），面板内可恢复 / 彻底删除
            var w = new ListPanelWindow("归档会话", "archived")
            {
                Owner = this,
                HostEngine = _engine
            };
            w.BindSessions(_engine.ArchivedSessions);
            _engine.LoadArchived();
            w.ShowDialog();
            // 面板把 ShowingArchived 拨到了 true；关掉必须拨回来，
            // 否则主列表接下来的回执都往 ArchivedSessions 里填，最近会话彻底不刷新
            _engine.LoadSessions();
        }

        /// <summary>
        /// 会话右键菜单：结构现场重建，对齐 Qt 0.6.191 rebuildMenu()。
        ///
        /// 为什么不在 XAML 里写成静态菜单 + 一堆 Visibility/Header 触发器：
        /// 「置顶 / 取消置顶」的文案、「本项在最近列表还是在回收站」，都随项状态变，
        /// 静态写出来是七八个互斥触发器，改一处要翻三遍，而且第一次弹菜单时
        /// 绑定还没走完一轮，容易闪一下旧文案。现场造菜单就是几步 new。
        /// </summary>
        /// <summary>
        /// 菜单项统一排版：MDL2 图标 + 中文文案。
        /// 不能只丢一个字符串给 Header —— WPF 画不出 emoji（.NET Framework 无 COLR 支持），
        /// 📌✏️📦 这些在菜单里会变成左侧一块空白，看着像缩进错位。
        /// 图标 Foreground 故意不设：让它继承 MenuItem 的 TextElement.Foreground，
        /// 禁用态 WPF 自动灰化、亮态自动跟主题色，不用手写两套。
        /// </summary>
        private static object MenuHeader(string glyph, string text)
        {
            var sp = new StackPanel { Orientation = Orientation.Horizontal };
            sp.Children.Add(new TextBlock
            {
                Text = glyph,
                FontFamily = Theme.FontIconFamily,
                FontSize = Theme.FontList,
                Margin = new Thickness(0, 0, 8, 0),
                VerticalAlignment = VerticalAlignment.Center
            });
            sp.Children.Add(new TextBlock
            {
                Text = text,
                VerticalAlignment = VerticalAlignment.Center
            });
            return sp;
        }

        private void OnSessionMenuOpened(object sender, RoutedEventArgs e)
        {
            var cm = sender as ContextMenu;
            if (cm == null) return;

            // ContextMenu 挂在 DataTemplate 的项根 Grid 上，它和主窗口不在同一棵视觉树
            // （Popup 断链），DataContext 的继承不可靠 —— 一律从 PlacementTarget 取。
            var fe = cm.PlacementTarget as FrameworkElement;
            var s = fe == null ? null : fe.DataContext as SessionInfo;

            // 分组标题行（今天 / 昨天 / 📌 置顶）不是会话，右键多半是点偏了一行。
            // 弹一个全是灰项的空壳菜单，不如干脆不弹。
            if (s == null || s.IsHeader || string.IsNullOrEmpty(s.Id))
            {
                cm.IsOpen = false;
                return;
            }

            // 截图样本只活在宿主里，引擎不认识这些 id，凡是会落库的动作一律禁用。
            // CI 出图要求菜单项全是亮色（好核对顺序与样式），FLARE_GUI_DEMO_REAL=1
            // 只在这种纯取证场景下放行；取证进程从不点任何菜单项，所以不会真落库。
            var isDemo = s.Id.StartsWith("demo-")
                         && Environment.GetEnvironmentVariable("FLARE_GUI_DEMO_REAL") != "1";
            var pinned = _engine.IsPinned(s.Id);

            cm.Items.Clear();

            // 逐项对齐 Qt 0.6.191 sessionlistwidget.cpp::rebuildMenu()：
            // 文案、顺序、分隔位、以及「归档箱里再删 = 彻底删除」这条都照搬。
            var miPin = new MenuItem { Header = pinned ? MenuHeader(Theme.Glyph.Unpin, "取消置顶") : MenuHeader(Theme.Glyph.Pin, "置顶会话") };
            miPin.IsEnabled = !isDemo;
            miPin.Click += (o, ev) => _engine.TogglePin(s.Id);
            cm.Items.Add(miPin);

            var miRename = new MenuItem { Header = MenuHeader(Theme.Glyph.Edit, "重命名会话"), InputGestureText = "F2" };
            miRename.IsEnabled = !isDemo;
            miRename.Click += (o, ev) => RenameSession(s);
            cm.Items.Add(miRename);

            cm.Items.Add(new Separator());

            if (_engine.ShowingArchived)
            {
                var miRestore = new MenuItem { Header = MenuHeader(Theme.Glyph.Undo, "恢复到最近会话") };
                miRestore.IsEnabled = !isDemo;
                miRestore.Click += (o, ev) => _engine.RestoreSession(s.Id);
                cm.Items.Add(miRestore);

                var miPurge = new MenuItem { Header = MenuHeader(Theme.Glyph.Cancel, "彻底删除（不可恢复）") };
                miPurge.IsEnabled = !isDemo;
                miPurge.Click += (o, ev) =>
                {
                    if (Dialog.Confirm(this, "彻底删除会话",
                            "彻底删除后无法恢复，确定要删除这个会话吗？", "删除", true))
                        _engine.PurgeSession(s.Id);
                };
                cm.Items.Add(miPurge);
            }
            else
            {
                var miArchive = new MenuItem { Header = MenuHeader(Theme.Glyph.Package, "归档到归档箱") };
                miArchive.IsEnabled = !isDemo;
                miArchive.Click += (o, ev) => _engine.EndSession(s.Id);
                cm.Items.Add(miArchive);
            }

            var miClear = new MenuItem { Header = MenuHeader(Theme.Glyph.Delete, "清空会话消息") };
            miClear.IsEnabled = !isDemo;
            miClear.Click += (o, ev) => ClearSession(s);
            cm.Items.Add(miClear);

            if (!_engine.ShowingArchived)
            {
                // Qt 里 archiveCurrent 与 deleteCurrent 落到同一条 end_session（都是软删），
                // 保留两项是因为「归档」和「丢进回收站」在中文里不是一回事。
                // EndSession 内部先 Unpin，和 Qt deleteCurrent 的 removeAll 一致。
                var miTrash = new MenuItem { Header = MenuHeader(Theme.Glyph.Delete, "移入回收站") };
                miTrash.IsEnabled = !isDemo;
                miTrash.Click += (o, ev) => _engine.EndSession(s.Id);
                cm.Items.Add(miTrash);
            }

            cm.Items.Add(new Separator());

            var miToggle = new MenuItem
            {
                Header = _engine.ShowingArchived ? MenuHeader(Theme.Glyph.Back, "返回最近会话") : MenuHeader(Theme.Glyph.Delete, "打开回收站")
            };
            miToggle.Click += (o, ev) =>
            {
                if (_engine.ShowingArchived) _engine.LoadSessions();
                else OpenArchivePanel();
            };
            cm.Items.Add(miToggle);
        }

        private void RenameSession(SessionInfo s)
        {
            var old = string.IsNullOrWhiteSpace(s.Title) ? s.Display : s.Title;
            var name = Dialog.Prompt(this, "重命名会话", old);
            if (name == null) return;
            name = name.Trim();
            // 空标题会让侧栏那行直接退化成预览首行，用户会以为没改成功；同名则是白跑一趟 IPC
            if (name.Length == 0 || name == old) return;
            _engine.RenameSession(s.Id, name);
        }

        private void ClearSession(SessionInfo s)
        {
            var ok = Dialog.Confirm(this, "清空会话消息",
                "将删除「" + s.Display + "」的全部对话内容，会话本身保留。此操作不可撤销。",
                "清空", true);
            if (ok) _engine.ClearSession(s.Id);
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
                Delay(5400, () =>
                {
                    LogShot("search \"" + searchKw + "\"");
                    SearchBox.Text = searchKw;   // → TextChanged → 250ms 防抖 → search_sessions
                    Delay(1500, () => Shot("02-search"));
                });
            }

            // FLARE_GUI_MENU=demo-2 → 打开该会话的右键菜单并单独出图。
            // RenderTargetBitmap 只渲染可视树，Popup 走独立 HwndSource 拍不到，
            // 所以菜单单独渲染一张 03-menu.png，主窗口另拍 03-menu-app.png，
            // 两张在 CI 里拼成对照图（左侧栏 + 右侧菜单）。
            // 必须排在搜索之前（3600ms，01-main 刚拍完、列表还是样本态）：
            // 搜索回执会把列表整段重建，之后再按 id 找索引就扑空了。
            var menuId = Environment.GetEnvironmentVariable("FLARE_GUI_MENU");
            if (!string.IsNullOrEmpty(menuId))
                Delay(3600, () => OpenMenuShot(menuId));

            // FLARE_GUI_SHOT_QUIT=1：取证序列跑完自动退出，让 CI 能无人值守跑完这一步。
            // 取最晚一次截图（02-search = 4200+1500ms）之后再留足余量，
            // 免得跟 Shot() 抢渲染；CI 侧仍保留超时强杀兜底。
            if (!string.IsNullOrEmpty(Environment.GetEnvironmentVariable("FLARE_GUI_SHOT_QUIT")))
            {
                Delay(15000, () =>
                {
                    LogShot("shot-quit");
                    try { Application.Current.Shutdown(); } catch { }
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

        // ================= 右键菜单取证辅助 =================

        /// <summary>选中目标会话 → 打开其右键菜单 → 记日志 → 拍两张图（主窗口 + 菜单本体）。</summary>
        private void OpenMenuShot(string id)
        {
            var idx = IndexOfSession(id);
            if (idx < 0) { LogShot("menu: 找不到会话 " + id + "（当前 " + SessionList.Items.Count + " 项）"); return; }
            SessionList.SelectedIndex = idx;
            SessionList.ScrollIntoView(SessionList.Items[idx]);
            SessionList.UpdateLayout();
            Delay(500, () =>
            {
                var cm = FindContextMenu(idx);
                if (cm == null) { LogShot("menu: " + id + " 容器上没挂 ContextMenu"); return; }
                // 代码里 IsOpen = true 不会自动定锚 —— 只有真的右键点击时
                // ContextMenuService 才写 PlacementTarget。不手动补上，
                // Opened 里从 PlacementTarget 取项会拿到 null，菜单就是空的。
                if (cm.PlacementTarget == null)
                    cm.PlacementTarget = SessionList.ItemContainerGenerator.ContainerFromIndex(idx) as UIElement;
                cm.Placement = System.Windows.Controls.Primitives.PlacementMode.Right;
                cm.IsOpen = true;            // → Opened → OnSessionMenuOpened 现场造项
                // Popup 的布局是异步的：IsOpen 之后同一帧里 MenuShell 的 ActualWidth
                // 还是 0（父级 PopupRoot 尚未 Measure），这时候 Render 出来的就是一条
                // 74x220 的白条 + 一个 F2 —— 03-menu.png 之前一直是这个鬼样子。
                // 等 600ms 让 Popup 完成布局再拍，拍完立刻关，不要拖到搜索那一步。
                Delay(600, () =>
                {
                    cm.UpdateLayout();
                    LogShot("menu: " + id + " items=" + cm.Items.Count + " → " + DescribeMenu(cm));
                    Shot("03-menu-app");
                    ShootMenu(cm, "03-menu");
                    cm.IsOpen = false;
                    LogShot("menu: 已关闭");
                });
            });
        }

        private int IndexOfSession(string id)
        {
            for (int i = 0; i < SessionList.Items.Count; i++)
            {
                var it = SessionList.Items[i] as SessionInfo;
                if (it != null && it.Id == id) return i;
            }
            return -1;
        }

        private ContextMenu FindContextMenu(int idx)
        {
            var item = SessionList.ItemContainerGenerator.ContainerFromIndex(idx) as DependencyObject;
            return item == null ? null : FindMenuIn(item);
        }

        private static ContextMenu FindMenuIn(DependencyObject d)
        {
            if (d == null) return null;
            var fe = d as FrameworkElement;
            if (fe != null && fe.ContextMenu != null) return fe.ContextMenu;
            int n = VisualTreeHelper.GetChildrenCount(d);
            for (int i = 0; i < n; i++)
            {
                var r = FindMenuIn(VisualTreeHelper.GetChild(d, i));
                if (r != null) return r;
            }
            return null;
        }

        /// <summary>把菜单项写成一行日志：能直接看出顺序、分隔符、哪些被禁用。</summary>
        private static string DescribeMenu(ContextMenu cm)
        {
            var sb = new StringBuilder();
            foreach (var o in cm.Items)
            {
                if (sb.Length > 0) sb.Append(" | ");
                var mi = o as MenuItem;
                if (mi != null) sb.Append(mi.IsEnabled ? "" : "[禁]").Append(mi.Header);
                else sb.Append("---");
            }
            return sb.ToString();
        }

        /// <summary>
        /// 单独渲染菜单。ContextMenu 自己的 visual parent 是 PopupRoot，
        /// 直接 Render 出来常常是空白，所以抓模板根 Border（x:Name="MenuShell"）。
        /// </summary>
        private void ShootMenu(ContextMenu cm, string name)
        {
            var dir = Environment.GetEnvironmentVariable("FLARE_GUI_SHOT");
            if (string.IsNullOrEmpty(dir)) return;
            try
            {
                Directory.CreateDirectory(dir);
                var shell = cm.Template == null ? null
                          : cm.Template.FindName("MenuShell", cm) as FrameworkElement;
                var target = shell ?? cm;
                target.UpdateLayout();
                var w = (int)Math.Ceiling(target.ActualWidth);
                var h = (int)Math.Ceiling(target.ActualHeight);
                LogShot("menu shot: shell=" + (shell != null) + " 首次尺寸 " + w + "x" + h);
                // 不只判 <=0：Popup 早拍时 shell 会量出一个 74x220 这种畸形的窄条
                // （宽度只容得下右侧 F2），照样能 Render，于是历史上「看着像成功」。
                // 菜单至少要有 120 宽 40 高，否则手动 Measure/Arrange 再来一遍。
                if (w < 120 || h < 40)
                {
                    // Popup 里的元素有时 UpdateLayout 拿不到尺寸（它不在主窗口的
                    // 布局树里）→ 手动量一遍再排一遍
                    target.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
                    target.Arrange(new Rect(target.DesiredSize));
                    target.UpdateLayout();
                    w = (int)Math.Ceiling(target.ActualWidth > 0 ? target.ActualWidth : target.DesiredSize.Width);
                    h = (int)Math.Ceiling(target.ActualHeight > 0 ? target.ActualHeight : target.DesiredSize.Height);
                    LogShot("menu shot: 复量尺寸 " + w + "x" + h);
                }
                if (w <= 0 || h <= 0) { LogShot("menu shot 尺寸为 0，跳过"); return; }
                var rtb = new RenderTargetBitmap(w, h, 96, 96, PixelFormats.Pbgra32);
                rtb.Render(target);
                var enc = new PngBitmapEncoder();
                enc.Frames.Add(BitmapFrame.Create(rtb));
                var p = Path.Combine(dir, name + ".png");
                using (var fs = File.Create(p)) enc.Save(fs);
                LogShot("menu shot " + p + " " + w + "x" + h);
            }
            catch (Exception ex)
            {
                LogShot("menu shot failed: " + ex.Message);
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
