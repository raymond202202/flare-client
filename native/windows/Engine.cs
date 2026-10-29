using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace FlareWin
{
    /// <summary>
    /// Engine.cs —— 引擎子进程桥。壳子只做两件事：画界面 + 收发 JSON Lines。
    /// 铁律（render-spec §0）：UI 不读 ~/.flare/.env、不内嵌引擎逻辑，一律 `flare server` 子进程。
    /// </summary>
    public class Engine : Observable
    {
        public enum EngineStatus
        {
            Stopped,
            Starting,
            Ready
        }

        private EngineStatus _status = EngineStatus.Stopped;
        public EngineStatus Status
        {
            get => _status;
            set => Set(ref _status, value);
        }

        private string _statusText = "未启动";
        public string StatusText
        {
            get => _statusText;
            set => Set(ref _statusText, value);
        }

        private string _engineVersion = "—";
        public string EngineVersion
        {
            get => _engineVersion;
            set => Set(ref _engineVersion, value);
        }

        private string _sessionID = "default";
        public string SessionID
        {
            get => _sessionID;
            set => Set(ref _sessionID, value);
        }

        private bool _busy;
        public bool Busy
        {
            get => _busy;
            set => Set(ref _busy, value);
        }

        private bool _showingArchived;
        public bool ShowingArchived
        {
            get => _showingArchived;
            set => Set(ref _showingArchived, value);
        }

        private string _modelName = "";
        public string ModelName
        {
            get => _modelName;
            set => Set(ref _modelName, value);
        }

        private string _providerName = "";
        public string ProviderName
        {
            get => _providerName;
            set => Set(ref _providerName, value);
        }

        private string _usageText = "";
        public string UsageText
        {
            get => _usageText;
            set => Set(ref _usageText, value);
        }

        private string _lastError;
        public string LastError
        {
            get => _lastError;
            set => Set(ref _lastError, value);
        }

        private string _enginePath = "";
        public string EnginePath
        {
            get => _enginePath;
            set => Set(ref _enginePath, value);
        }

        private ConfirmRequest _pendingConfirm;
        public ConfirmRequest PendingConfirm
        {
            get => _pendingConfirm;
            set => Set(ref _pendingConfirm, value);
        }

        private bool _cancelRequested;

        /// <summary>引擎回过 cancelled 帧即置 true（自测用）。</summary>
        public bool CancelledOnce { get; private set; }

        /// <summary>客户端版本：编译期常量，与运行时 version 帧的引擎版本是两个来源，不得合并显示。</summary>
        public string ClientVersion =>
            typeof(Engine).Assembly.GetName().Version.ToString(3);

        public ObservableCollection<ChatBlock> Blocks { get; } = new ObservableCollection<ChatBlock>();
        public ObservableCollection<SessionInfo> Sessions { get; } = new ObservableCollection<SessionInfo>();
        public ObservableCollection<SessionInfo> ArchivedSessions { get; } = new ObservableCollection<SessionInfo>();
        public ObservableCollection<MemoryItem> Memories { get; } = new ObservableCollection<MemoryItem>();
        public ObservableCollection<ToolSpec> Tools { get; } = new ObservableCollection<ToolSpec>();

        private Process _proc;
        private StreamWriter _stdin;
        private readonly Queue<string> _stderrTail = new Queue<string>();
        private static readonly object _logLock = new object();

        /// <summary>调试日志：仅当 FLARE_GUI_LOG 指向可写文件时启用，正常用户零文件产出。</summary>
        private static void Dbg(string s)
        {
            var p = Environment.GetEnvironmentVariable("FLARE_GUI_LOG");
            if (string.IsNullOrEmpty(p)) return;
            try
            {
                lock (_logLock)
                    File.AppendAllText(p, DateTime.Now.ToString("HH:mm:ss.fff") + " " + s + "\n", Encoding.UTF8);
            }
            catch
            {
                // 日志永不影响主流程
            }
        }

        // MARK: - 生命周期

        public void Start()
        {
            if (_proc != null) return;
            var exe = LocateEngine();
            if (exe == null)
            {
                Status = EngineStatus.Stopped;
                StatusText = "找不到 flare 引擎可执行文件";
                AppendSystem("未找到 flare 引擎。请把引擎放到程序目录下的 engine\\flare.exe，或设置环境变量 FLARE_BIN。");
                return;
            }
            EnginePath = exe;
            Dbg("== spawn " + exe + " server");

            var psi = new ProcessStartInfo
            {
                FileName = exe,
                Arguments = "server",
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                StandardOutputEncoding = new UTF8Encoding(false),
                StandardErrorEncoding = new UTF8Encoding(false),
                WorkingDirectory = Path.GetDirectoryName(exe) ?? Environment.CurrentDirectory
            };
            var p = new Process { StartInfo = psi, EnableRaisingEvents = true };
            p.OutputDataReceived += (s, e) =>
            {
                if (e.Data == null) return;
                var line = e.Data.Trim();
                if (line.Length == 0) return;
                Ui(() => Handle(line));
            };
            p.ErrorDataReceived += (s, e) =>
            {
                if (e.Data == null) return;
                lock (_stderrTail)
                {
                    _stderrTail.Enqueue(e.Data);
                    while (_stderrTail.Count > 40) _stderrTail.Dequeue();
                }
            };
            p.Exited += (s, e) => Ui(() =>
            {
                Status = EngineStatus.Stopped;
                StatusText = "引擎已退出（code " + SafeExitCode(p) + "）";
                Busy = false;
                _proc = null;
            });

            try
            {
                p.Start();
            }
            catch (Exception ex)
            {
                Status = EngineStatus.Stopped;
                StatusText = "引擎启动失败：" + ex.Message;
                AppendSystem(StatusText);
                return;
            }
            p.BeginOutputReadLine();
            p.BeginErrorReadLine();
            _proc = p;
            _stdin = p.StandardInput;
            _stdin.AutoFlush = true;
            Status = EngineStatus.Starting;
            StatusText = "正在启动引擎…";

            Send(New("version"));
            // 侧栏首屏用 recent_sessions 而非 list_sessions：只有它带 preview 字段
            Send(New("recent_sessions"));
            Send(New("tools"));
            Send(New("models"));

            var smoke = Environment.GetEnvironmentVariable("FLARE_GUI_SMOKE");
            if (!string.IsNullOrEmpty(smoke))
            {
                Dbg("== smoke enabled");
                Task.Run(async () =>
                {
                    await Task.Delay(3000);
                    Ui(() => SendChat(smoke));
                    var cs = Environment.GetEnvironmentVariable("FLARE_GUI_AUTOCANCEL");
                    if (double.TryParse(cs, out var secs) && secs > 0)
                    {
                        await Task.Delay(TimeSpan.FromSeconds(secs));
                        Ui(Cancel);
                    }
                });
            }
        }

        private static string SafeExitCode(Process p)
        {
            try { return p.ExitCode.ToString(); }
            catch { return "?"; }
        }

        public void Stop()
        {
            var p = _proc;
            if (p == null) return;
            try
            {
                Send(New("shutdown"));
                if (!p.WaitForExit(1500))
                    p.Kill();
            }
            catch
            {
                try { p.Kill(); } catch { }
            }
            _proc = null;
            _stdin = null;
            Status = EngineStatus.Stopped;
        }

        // MARK: - 发送

        public void Send(JObject obj)
        {
            var w = _stdin;
            if (w == null) return;
            var line = obj.ToString(Formatting.None);
            Dbg(">> " + line);
            try
            {
                w.WriteLine(line);
            }
            catch (Exception ex)
            {
                Dbg("!! stdin write failed: " + ex.Message);
            }
        }

        private static JObject New(string type) => new JObject { ["type"] = type };

        public void SendChat(string text)
        {
            var t = (text ?? "").Trim();
            if (t.Length == 0 || Busy) return;
            Blocks.Add(new ChatBlock { Kind = BlockKind.User, Text = t });
            Blocks.Add(new ChatBlock { Kind = BlockKind.Assistant, Author = "Flare：", Streaming = true });
            Busy = true;
            _cancelRequested = false;
            StatusText = "生成中…";
            var o = New("chat");
            o["input"] = t;
            o["sessionId"] = SessionID;
            Send(o);
        }

        public void Cancel()
        {
            if (!Busy) return;
            _cancelRequested = true;
            StatusText = "正在停止…";
            var o = New("cancel");
            o["sessionId"] = SessionID;
            Send(o);
        }

        public void AnswerConfirm(string decision, string alternative = "")
        {
            var c = PendingConfirm;
            if (c == null) return;
            var o = New("confirm_response");
            o["id"] = c.Id;
            o["decision"] = decision;
            if (!string.IsNullOrEmpty(alternative)) o["alternative"] = alternative;
            Send(o);
            PendingConfirm = null;
        }

        public void LoadSessions() { ShowingArchived = false; Send(New("recent_sessions")); }
        public void LoadArchived() { ShowingArchived = true; Send(New("archived_sessions")); }
        public void LoadModels() => Send(New("models"));
        public void LoadMemories() => Send(New("list_memories"));
        public void LoadTools() => Send(New("tools"));
        public void LoadUsage() => Send(New("get_usage"));

        public void LoadMessages()
        {
            var o = New("get_messages");
            o["sessionId"] = SessionID;
            Send(o);
        }

        public void NewSession() => Send(New("new_session"));

        public void SwitchSession(string id)
        {
            if (string.IsNullOrEmpty(id) || id == SessionID) return;
            SessionID = id;
            Blocks.Clear();
            var o = New("get_messages");
            o["sessionId"] = id;
            Send(o);
        }

        public void SaveMemory(string content)
        {
            var t = (content ?? "").Trim();
            if (t.Length == 0) return;
            var o = New("save_memory");
            o["content"] = t;
            Send(o);
            Send(New("list_memories"));
        }

        public void DeleteMemory(string id)
        {
            var o = New("delete_memory");
            o["id"] = id;
            Send(o);
            Send(New("list_memories"));
        }

        public void RenameSession(string id, string title)
        {
            var o = New("update_session_title");
            o["sessionId"] = id;
            o["title"] = title;
            Send(o);
        }

        public void DeleteSession(string id)
        {
            var o = New("delete_session");
            o["sessionId"] = id;
            Send(o);
            if (id == SessionID)
            {
                SessionID = "default";
                Blocks.Clear();
            }
            Send(New("recent_sessions"));
        }

        public void ClearMessages()
        {
            var o = New("clear_messages");
            o["sessionId"] = SessionID;
            Send(o);
        }

        // MARK: - 接收

        private void Handle(string line)
        {
            Dbg("<< " + line);
            JObject obj;
            try
            {
                obj = JObject.Parse(line);
            }
            catch
            {
                return;
            }
            var type = (string)obj["type"];
            if (string.IsNullOrEmpty(type)) return;

            switch (type)
            {
                case "text":
                {
                    // 帧名在 render-spec v1.0 已从 chunk 纠正为 text，两种键都兼容
                    var chunk = (string)obj["content"] ?? (string)obj["text"] ?? "";
                    AppendToStreamingFlare(chunk);
                    break;
                }

                case "done":
                {
                    FinishStreaming();
                    Dbg("done: blocks=" + Blocks.Count);
                    var sid = (string)obj["sessionId"];
                    if (!string.IsNullOrEmpty(sid)) SessionID = sid;
                    Busy = false;
                    StatusText = "Flare 引擎已连接";
                    Send(New("recent_sessions"));
                    if (_cancelRequested)
                    {
                        AppendSystem("⏹ 已停止生成");
                        _cancelRequested = false;
                    }
                    break;
                }

                case "tool_call":
                {
                    var name = (string)obj["toolName"] ?? "tool";
                    var args = Pretty(obj["args"]);
                    Blocks.Add(new ChatBlock { Kind = BlockKind.Tool, Author = name + "(…)", Text = args });
                    FinishStreaming();
                    break;
                }

                case "tool_result":
                {
                    var name = (string)obj["toolName"] ?? "tool";
                    var outp = (string)obj["content"] ?? "";
                    var isErr = obj["isErr"] != null && (bool)obj["isErr"];
                    Blocks.Add(new ChatBlock { Kind = BlockKind.ToolResult, Author = name + " 结果", Text = outp });
                    break;
                }

                case "confirm":
                {
                    var id = (string)obj["id"] ?? (string)obj["confirmId"] ?? "";
                    var name = (string)obj["name"] ?? "tool";
                    var desc = (string)obj["description"] ?? "";
                    PendingConfirm = new ConfirmRequest
                    {
                        Id = id,
                        Tool = name,
                        Detail = string.IsNullOrEmpty(desc) ? Pretty(obj["args"]) : desc
                    };
                    break;
                }

                case "confirm_result":
                {
                    var allow = obj["allow"] != null && (bool)obj["allow"];
                    AppendSystem(allow ? "✅ 已放行：" + ((string)obj["confirmId"] ?? "") : "⛔ 已拒绝本次工具调用");
                    break;
                }

                case "cancelled":
                {
                    FinishStreaming();
                    Busy = false;
                    StatusText = "已停止";
                    // 提示在这里给过了，done 的兜底分支不要再重复一遍
                    _cancelRequested = false;
                    CancelledOnce = true;
                    AppendSystem("⏹ Flare：已停止生成");
                    Send(New("recent_sessions"));
                    break;
                }

                case "error":
                {
                    FinishStreaming();
                    Busy = false;
                    var msg = (string)obj["message"] ?? "引擎返回未知错误";
                    LastError = msg;
                    StatusText = "错误";
                    AppendSystem("⚠️ " + msg);
                    break;
                }

                case "sessions":
                case "recent_sessions":
                    FillSessions(Sessions, obj);
                    ShowingArchived = false;
                    break;

                case "archived_sessions":
                    FillSessions(ArchivedSessions, obj);
                    ShowingArchived = true;
                    break;

                case "session_created":
                {
                    var sid = (string)obj["sessionId"];
                    if (!string.IsNullOrEmpty(sid)) SessionID = sid;
                    Blocks.Clear();
                    Send(New("recent_sessions"));
                    break;
                }

                case "session_updated":
                    Send(New("recent_sessions"));
                    break;

                case "messages":
                    FillMessages(obj);
                    break;

                case "memories":
                    FillMemories(obj);
                    break;

                case "tools":
                    FillTools(obj);
                    break;

                case "version":
                    EngineVersion = (string)obj["engine"] ?? (string)obj["version"] ?? "—";
                    Status = EngineStatus.Ready;
                    StatusText = "Flare 引擎已连接";
                    break;

                case "usage":
                    UsageText = ParseUsage(obj);
                    break;

                case "models":
                {
                    var cfg = obj["configured"] as JObject;
                    var main = cfg?["main"] as JObject;
                    if (main != null)
                    {
                        ModelName = (string)main["model"] ?? "";
                        ProviderName = (string)main["provider"] ?? "";
                    }
                    if (Status != EngineStatus.Ready) Status = EngineStatus.Ready;
                    // models 帧到齐＝引擎已就绪，状态栏文案统一在这里定稿
                    StatusText = "Flare 引擎已连接";
                    break;
                }

                case "messages_cleared":
                    Blocks.Clear();
                    break;

                case "memory_deleted":
                    Send(New("list_memories"));
                    break;

                case "pong":
                case "ok":
                case "confirm_status":
                    break;

                default:
                    // 向前兼容铁律：未知 type 静默忽略，不得崩、不得报错
                    break;
            }
        }

        // MARK: - 流式块

        private void AppendToStreamingFlare(string chunk)
        {
            if (string.IsNullOrEmpty(chunk)) return;
            if (Blocks.Count > 0)
            {
                var last = Blocks[Blocks.Count - 1];
                if (last.Kind == BlockKind.Assistant && last.Streaming)
                {
                    last.Append(chunk);
                    return;
                }
            }
            Blocks.Add(new ChatBlock { Kind = BlockKind.Assistant, Author = "Flare：", Text = chunk, Streaming = true });
        }

        private void FinishStreaming()
        {
            foreach (var b in Blocks)
                if (b.Streaming) b.Streaming = false;
        }

        private void AppendSystem(string text)
        {
            Blocks.Add(new ChatBlock { Kind = BlockKind.System, Text = text });
        }

        // MARK: - 解析辅助

        private static string Pretty(JToken t)
        {
            if (t == null || t.Type == JTokenType.Null) return "";
            if (t.Type == JTokenType.String) return (string)t;
            return t.ToString(Formatting.Indented);
        }

        private void FillSessions(ObservableCollection<SessionInfo> target, JObject obj)
        {
            target.Clear();
            if (!(obj["sessions"] is JArray arr)) return;
            foreach (var it in arr)
            {
                var id = (string)it["id"] ?? (string)it["sessionId"] ?? "";
                if (string.IsNullOrEmpty(id)) continue;
                var title = (string)it["title"] ?? (string)it["name"];
                if (string.IsNullOrEmpty(title)) title = "未命名会话";
                target.Add(new SessionInfo
                {
                    Id = id,
                    Title = title,
                    Preview = (string)it["preview"] ?? "",
                    UpdatedAt = (string)it["updatedAt"] ?? (string)it["updated_at"] ?? "",
                    Count = it["count"] != null ? (int?)it["count"] ?? 0 : 0
                });
            }
        }

        private void FillMemories(JObject obj)
        {
            Memories.Clear();
            var arr = (obj["memories"] as JArray) ?? (obj["items"] as JArray);
            if (arr == null) return;
            foreach (var it in arr)
            {
                var id = (string)it["id"] ?? "";
                var content = (string)it["content"] ?? (string)it["text"] ?? "";
                if (id.Length == 0 && content.Length == 0) continue;
                Memories.Add(new MemoryItem { Id = id, Content = content, CreatedAt = (string)it["createdAt"] ?? "" });
            }
        }

        private void FillTools(JObject obj)
        {
            Tools.Clear();
            if (!(obj["tools"] is JArray arr)) return;
            foreach (var it in arr)
            {
                var name = (string)it["name"] ?? "";
                if (name.Length == 0) continue;
                Tools.Add(new ToolSpec { Name = name, Description = (string)it["description"] ?? "" });
            }
        }

        private void FillMessages(JObject obj)
        {
            Blocks.Clear();
            if (!(obj["messages"] is JArray arr)) return;
            foreach (var m in arr)
            {
                var role = (string)m["role"] ?? "";
                var content = (string)m["content"] ?? "";
                switch (role)
                {
                    case "user":
                        Blocks.Add(new ChatBlock { Kind = BlockKind.User, Text = content });
                        break;
                    case "assistant":
                        if (content.Length > 0)
                            Blocks.Add(new ChatBlock { Kind = BlockKind.Assistant, Author = "Flare：", Text = content });
                        break;
                    case "tool":
                        Blocks.Add(new ChatBlock { Kind = BlockKind.Tool, Author = (string)m["name"] ?? "tool", Text = content });
                        break;
                }
            }
        }

        private static string ParseUsage(JObject obj)
        {
            var st = obj["stats"] as JObject;
            if (st == null) return "";
            var total = st["totalTokens"] != null ? (long)st["totalTokens"] : 0;
            var reqs = st["requestCount"] != null ? (long)st["requestCount"] : 0;
            return "累计 " + reqs + " 次请求 / " + total + " tokens";
        }

        // MARK: - 引擎定位

        private static string LocateEngine()
        {
            var ov = Environment.GetEnvironmentVariable("FLARE_BIN");
            if (!string.IsNullOrEmpty(ov) && File.Exists(ov)) return ov;

            var cands = new List<string>();
            var baseDir = AppDomain.CurrentDomain.BaseDirectory;
            // GUI 包内置引擎固定在 <程序目录>\engine\flare.exe
            cands.Add(Path.Combine(baseDir, "engine", "flare.exe"));
            cands.Add(Path.Combine(baseDir, "flare.exe"));
            var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            cands.Add(Path.Combine(home, ".flare", "install", "flare.exe"));
            cands.Add(Path.Combine(home, ".flare", "flare.exe"));
            if (Environment.Is64BitOperatingSystem)
            {
                var pf = Environment.GetEnvironmentVariable("ProgramFiles") ?? @"C:\Program Files";
                cands.Add(Path.Combine(pf, "Flare", "flare.exe"));
            }
            var path = Environment.GetEnvironmentVariable("PATH") ?? "";
            foreach (var d in path.Split(';'))
            {
                if (string.IsNullOrWhiteSpace(d)) continue;
                cands.Add(Path.Combine(d.Trim(), "flare.exe"));
            }
            foreach (var c in cands)
            {
                try
                {
                    if (File.Exists(c)) return c;
                }
                catch
                {
                    // 非法路径忽略
                }
            }
            return null;
        }

        /// <summary>所有集合/属性变更都必须在 UI 线程执行。</summary>
        private static void Ui(Action a)
        {
            var app = Application.Current;
            if (app == null || app.Dispatcher.CheckAccess()) a();
            else app.Dispatcher.BeginInvoke(a);
        }
    }
}
