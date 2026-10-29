using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace FlareWin
{
    /// <summary>所有可绑定模型的最小 INPC 基类。</summary>
    public abstract class Observable : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;

        protected void Raise([CallerMemberName] string name = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }

        protected bool Set<T>(ref T field, T value, [CallerMemberName] string name = null)
        {
            if (Equals(field, value)) return false;
            field = value;
            Raise(name);
            return true;
        }
    }

    public enum BlockKind
    {
        User,
        Assistant,
        System,
        Tool,
        ToolResult
    }

    /// <summary>
    /// 聊天区的一个消息块。render-spec §4：流式增量（text 帧）**追加到同一个 Flare 块**，
    /// 绝不新建块；作者名只在块首写一次。
    /// </summary>
    public class ChatBlock : Observable
    {
        private string _text = "";
        private bool _streaming;

        public BlockKind Kind { get; set; }

        /// <summary>作者名：用户消息为空（右对齐气泡不显示名），Flare 块显示 "Flare："。</summary>
        public string Author { get; set; } = "";

        public string Text
        {
            get => _text;
            set => Set(ref _text, value);
        }

        /// <summary>true 时尾部显示一个闪烁光标（render-spec §5）。</summary>
        public bool Streaming
        {
            get => _streaming;
            set => Set(ref _streaming, value);
        }

        public string Time { get; set; } = DateTime.Now.ToString("HH:mm");

        public void Append(string delta)
        {
            if (string.IsNullOrEmpty(delta)) return;
            Text += delta;
        }
    }

    public class SessionInfo
    {
        public string Id { get; set; } = "";
        public string Title { get; set; } = "";
        public string Preview { get; set; } = "";
        public int Count { get; set; }
        public string UpdatedAt { get; set; } = "";

        /// <summary>侧栏列表实际显示的文本：优先标题，退化到预览首行。</summary>
        public string Display
        {
            get
            {
                var t = string.IsNullOrWhiteSpace(Title) ? Preview : Title;
                if (string.IsNullOrWhiteSpace(t)) t = "(空会话)";
                t = t.Replace("\r", " ").Replace("\n", " ").Trim();
                return t.Length > 42 ? t.Substring(0, 42) + "…" : t;
            }
        }
    }

    public class MemoryItem
    {
        public string Id { get; set; } = "";
        public string Content { get; set; } = "";
        public string CreatedAt { get; set; } = "";
    }

    public class ToolSpec
    {
        public string Name { get; set; } = "";
        public string Description { get; set; } = "";
    }

    /// <summary>引擎请求用户确认（危险工具调用前）。render-spec §4.2：绝不自动允许。</summary>
    public class ConfirmRequest
    {
        public string Id { get; set; } = "";
        public string Tool { get; set; } = "";
        public string Detail { get; set; } = "";
    }
}
