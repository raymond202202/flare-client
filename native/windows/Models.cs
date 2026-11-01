using System;
using System.ComponentModel;
using System.Globalization;
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

        /// <summary>
        /// 侧栏列表实际显示的文本：优先标题，退化到预览首行。
        /// 不在宿主侧按字符数硬裁 —— 侧栏宽度会变（拖分隔条 / 缩窗口），
        /// 提前裁掉的文本在变宽后不会自己长回来，只能永远是「用一句话介绍…」。
        /// 截断交给 TextTrimming 在绘制时按可用宽度做，与 Qt 侧 0.6.191 同一条规矩。
        /// </summary>
        public string Display
        {
            get
            {
                var t = string.IsNullOrWhiteSpace(Title) ? Preview : Title;
                if (string.IsNullOrWhiteSpace(t)) t = "新会话";
                return t.Replace("\r", " ").Replace("\n", " ").Trim();
            }
        }

        /// <summary>
        /// 右端时间列：今天只留 HH:mm，今年 MM-dd HH:mm，跨年压成两位年 yy-MM-dd HH:mm。
        /// 口径逐字对齐 Qt 侧 FlareTime::listStampCompact —— 两端必须同一份时间语义，
        /// 否则同一条会话在两平台上显示的时间不一样，等于两个事实。
        /// 分钟不能省：会话多半挤在同一天，只到日期的话整列时间全是「今天」，等于白放。
        /// </summary>
        public string Stamp
        {
            get
            {
                var dt = ParseUpdated();
                if (dt == null) return "";
                var today = DateTime.Now.Date;
                if (dt.Value.Date == today) return dt.Value.ToString("HH:mm");
                if (dt.Value.Year == today.Year) return dt.Value.ToString("MM-dd HH:mm");
                return dt.Value.ToString("yy-MM-dd HH:mm");
            }
        }

        /// <summary>完整时间，留给 tooltip —— 紧凑口径会丢掉年份和秒，悬停时补全。</summary>
        public string FullStamp
        {
            get
            {
                var dt = ParseUpdated();
                return dt == null ? "" : dt.Value.ToString("yyyy-MM-dd HH:mm");
            }
        }

        private DateTime? ParseUpdated()
        {
            if (string.IsNullOrWhiteSpace(UpdatedAt)) return null;
            DateTime dt;
            if (!DateTime.TryParse(UpdatedAt, CultureInfo.InvariantCulture,
                                   DateTimeStyles.RoundtripKind, out dt))
                return null;
            // 引擎给的是 UTC（带 Z）：不转本地时间的话，晚上 8 点后的会话会被显示成第二天。
            if (dt.Kind == DateTimeKind.Utc) dt = dt.ToLocalTime();
            return dt;
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
