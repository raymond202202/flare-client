using System;
using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace FlareWin
{
    /// <summary>
    /// 通用列表面板：归档会话 / 记忆 / 技能 三个次级面板共用一套外观。
    /// 与 macOS 侧的 PanelView 保持同一视觉语言（render-spec §6）。
    /// </summary>
    public class ListPanelWindow : Window
    {
        private readonly string _mode;
        private Engine _engine;
        private readonly StackPanel _host = new StackPanel();
        private readonly TextBox _newMemory = new TextBox();

        public ListPanelWindow(string title, string mode)
        {
            _mode = mode;
            Title = title + " · Flare";
            Width = 560;
            Height = 560;
            WindowStartupLocation = WindowStartupLocation.CenterOwner;
            Background = Theme.BgWindow;
            FontFamily = new FontFamily(Theme.FontUi);
            UseLayoutRounding = true;

            var root = new Grid { Margin = new Thickness(18) };
            root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            root.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

            var head = new TextBlock
            {
                Text = title,
                FontSize = Theme.FontSettingsTitle,
                FontWeight = FontWeights.SemiBold,
                Foreground = Theme.TextMain,
                Margin = new Thickness(0, 0, 0, 12)
            };
            Grid.SetRow(head, 0);
            root.Children.Add(head);

            var scroll = new ScrollViewer
            {
                VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
                HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
                Content = _host
            };
            Grid.SetRow(scroll, 1);
            root.Children.Add(scroll);

            if (_mode == "memories")
            {
                var add = new Grid { Margin = new Thickness(0, 12, 0, 0) };
                add.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                add.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                _newMemory.Style = (Style)Application.Current.Resources["ChatInput"];
                _newMemory.Height = 38;
                _newMemory.Padding = new Thickness(10, 6, 10, 6);
                _newMemory.Background = Theme.BgSoft;
                var shell = new Border
                {
                    Background = Theme.BgSoft,
                    BorderBrush = Theme.BorderSoft2,
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(9),
                    Child = _newMemory,
                    Margin = new Thickness(0, 0, 8, 0)
                };
                Grid.SetColumn(shell, 0);
                add.Children.Add(shell);

                var btn = new Button
                {
                    Content = "记住",
                    Style = (Style)Application.Current.Resources["PrimaryBtn"]
                };
                btn.Click += (s, e) => AddMemory();
                Grid.SetColumn(btn, 1);
                add.Children.Add(btn);
                Grid.SetRow(add, 2);
                root.Children.Add(add);
            }

            Content = root;

            Loaded += (s, e) =>
            {
                // 面板自己也从 Owner 的 DataContext 兜一层，但归档面板是主窗口
                // 直接 new 出来的，显式给一份 HostEngine 更稳（Owner.DataContext 可能是别的）
                if (_engine == null) _engine = (Owner as Window)?.DataContext as Engine;
                Refresh();
            };
        }

        /// <summary>显式传入引擎（归档面板在 ShowDialog 前设置，比靠 Owner.DataContext 稳）。</summary>
        public Engine HostEngine
        {
            get { return _engine; }
            set { _engine = value; }
        }

        public void BindSessions(ObservableCollection<SessionInfo> src)
        {
            Loaded += (s, e) =>
            {
                FillSessions(src);
                // 归档集合由引擎回执重建（不是原地改），恢复/彻底删除后必须跟着重绘
                src.CollectionChanged += (s2, e2) => FillSessions(src);
            };
        }

        public void BindMemories(ObservableCollection<MemoryItem> src, Engine engine)
        {
            _engine = engine;
            Loaded += (s, e) =>
            {
                FillMemories(src);
                src.CollectionChanged += (s2, e2) => FillMemories(src);
            };
        }

        public void BindTools(ObservableCollection<ToolSpec> src)
        {
            Loaded += (s, e) =>
            {
                FillTools(src);
                src.CollectionChanged += (s2, e2) => FillTools(src);
            };
        }

        private void Refresh()
        {
            if (_engine == null) return;
            if (_mode == "archived") FillSessions(_engine.ArchivedSessions);
            else if (_mode == "memories") FillMemories(_engine.Memories);
            else if (_mode == "tools") FillTools(_engine.Tools);
        }

        private void FillSessions(ObservableCollection<SessionInfo> src)
        {
            if (_mode != "archived") return;
            _host.Children.Clear();
            if (src.Count == 0)
            {
                _host.Children.Add(Empty("没有归档会话"));
                return;
            }
            foreach (var s in src)
            {
                var card = Card();
                var tb = new TextBlock
                {
                    Text = s.Display,
                    FontSize = Theme.FontBody,
                    Foreground = Theme.TextMain,
                    TextWrapping = TextWrapping.Wrap
                };
                var sub = new TextBlock
                {
                    Text = (string.IsNullOrEmpty(s.Preview) ? "" : s.Preview + "  ·  ") + s.Stamp,
                    FontSize = Theme.FontFootnote,
                    Foreground = Theme.TextMuted,
                    Margin = new Thickness(0, 4, 0, 0),
                    TextTrimming = TextTrimming.CharacterEllipsis
                };
                var st = new StackPanel();
                st.Children.Add(tb);
                st.Children.Add(sub);

                // 归档箱里两个动作：恢复（回最近会话）/ 彻底删除（落库不可恢复）。
                // 都落在引擎既有命令上（restore_session / delete_session），面板只做确认。
                var acts = new StackPanel
                {
                    Orientation = Orientation.Horizontal,
                    VerticalAlignment = VerticalAlignment.Top,
                    Margin = new Thickness(10, -2, 0, 0)
                };
                var id = s.Id;
                var restore = new Button
                {
                    Content = "恢复",
                    Style = (Style)Application.Current.Resources["GhostBtn"],
                    Padding = new Thickness(11, 4, 11, 4)
                };
                restore.Click += (o, ev) => _engine?.RestoreSession(id);
                var purge = new Button
                {
                    Content = "彻底删除",
                    Style = (Style)Application.Current.Resources["StopBtn"],
                    Padding = new Thickness(11, 4, 11, 4),
                    Margin = new Thickness(8, 0, 0, 0)
                };
                purge.Click += (o, ev) =>
                {
                    if (Dialog.Confirm(this, "彻底删除会话",
                            "「" + s.Display + "」将被永久删除，无法恢复。", "删除", true))
                        _engine?.PurgeSession(id);
                };
                acts.Children.Add(restore);
                acts.Children.Add(purge);

                var row = new Grid();
                row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                Grid.SetColumn(st, 0);
                row.Children.Add(st);
                Grid.SetColumn(acts, 1);
                row.Children.Add(acts);

                card.Child = row;
                card.Cursor = Cursors.Hand;
                card.MouseLeftButtonUp += (o, ev) =>
                {
                    _engine?.SwitchSession(id);
                    Close();
                };
                _host.Children.Add(card);
            }
        }

        private void FillMemories(ObservableCollection<MemoryItem> src)
        {
            if (_mode != "memories") return;
            _host.Children.Clear();
            if (src.Count == 0)
            {
                _host.Children.Add(Empty("还没有记忆。Flare 会记住你让它记下的事。"));
                return;
            }
            foreach (var m in src)
            {
                var card = Card();
                var g = new Grid();
                g.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                g.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                var tb = new TextBlock
                {
                    Text = m.Content,
                    FontSize = Theme.FontBody,
                    Foreground = Theme.TextMain,
                    TextWrapping = TextWrapping.Wrap
                };
                Grid.SetColumn(tb, 0);
                g.Children.Add(tb);

                var del = new Button
                {
                    Content = "删除",
                    Style = (Style)Application.Current.Resources["GhostBtn"],
                    VerticalAlignment = VerticalAlignment.Top,
                    Margin = new Thickness(10, -2, 0, 0)
                };
                var id = m.Id;
                del.Click += (o, ev) => _engine?.DeleteMemory(id);
                Grid.SetColumn(del, 1);
                g.Children.Add(del);

                card.Child = g;
                _host.Children.Add(card);
            }
        }

        private void FillTools(ObservableCollection<ToolSpec> src)
        {
            if (_mode != "tools") return;
            _host.Children.Clear();
            if (src.Count == 0)
            {
                _host.Children.Add(Empty("技能列表获取中…"));
                return;
            }
            foreach (var t in src)
            {
                var card = Card();
                var st = new StackPanel();
                st.Children.Add(new TextBlock
                {
                    Text = t.Name,
                    FontFamily = new FontFamily(Theme.FontMono),
                    FontSize = Theme.FontList,
                    Foreground = Theme.Orange,
                    FontWeight = FontWeights.SemiBold
                });
                if (!string.IsNullOrEmpty(t.Description))
                {
                    st.Children.Add(new TextBlock
                    {
                        Text = t.Description,
                        FontSize = Theme.FontSmall,
                        Foreground = Theme.TextSoft,
                        TextWrapping = TextWrapping.Wrap,
                        Margin = new Thickness(0, 4, 0, 0)
                    });
                }
                card.Child = st;
                _host.Children.Add(card);
            }
        }

        private static Border Card()
        {
            return new Border
            {
                Background = Theme.BgSoft,
                BorderBrush = Theme.BorderSoft,
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(10),
                Padding = new Thickness(14, 12, 14, 12),
                Margin = new Thickness(0, 0, 0, 8)
            };
        }

        private static TextBlock Empty(string s)
        {
            return new TextBlock
            {
                Text = s,
                FontSize = Theme.FontSmall,
                Foreground = Theme.TextFaint,
                TextWrapping = TextWrapping.Wrap,
                Margin = new Thickness(2, 6, 0, 0)
            };
        }

        private void AddMemory()
        {
            var t = _newMemory.Text;
            if (string.IsNullOrWhiteSpace(t)) return;
            _engine?.SaveMemory(t);
            _newMemory.Clear();
        }
    }
}
