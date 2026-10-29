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
                _engine = (Owner as Window)?.DataContext as Engine;
                Refresh();
            };
        }

        public void BindSessions(ObservableCollection<SessionInfo> src)
        {
            Loaded += (s, e) => FillSessions(src);
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
                    Text = (string.IsNullOrEmpty(s.Preview) ? "" : s.Preview + "  ·  ") + s.UpdatedAt,
                    FontSize = Theme.FontFootnote,
                    Foreground = Theme.TextMuted,
                    Margin = new Thickness(0, 4, 0, 0),
                    TextTrimming = TextTrimming.CharacterEllipsis
                };
                var st = new StackPanel();
                st.Children.Add(tb);
                st.Children.Add(sub);
                card.Child = st;
                var id = s.Id;
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
