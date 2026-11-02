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

        // ================= 搜索 / 置顶（本地偏好，一律不进引擎库） =================

        /// <summary>
        /// 搜索框里的**当前**文本。每次 TextChanged 由宿主同步进来。
        /// 与 _inflightQuery 分开是有意的：它决定「分组标题要不要插」，而 Qt 判的
        /// 也是搜索框当前文本（sessionlistwidget.cpp:604）；用 inflight 的话，
        /// 防抖窗口里输入框已变、关键词还没发出，会漏插或多插一次标题。
        /// </summary>
        private string _searchQuery = "";
        public string SearchQuery
        {
            get { return _searchQuery; }
            set { _searchQuery = value ?? ""; }
        }

        /// <summary>已发出、尚未回执的搜索关键词，用于丢弃迟到回执。</summary>
        private string _inflightQuery = "";

        /// <summary>置顶会话 id 集合。落 INI 的 [sessions] pinned，与 Qt 客户端同路径同键。</summary>
        private readonly HashSet<string> _pinned = new HashSet<string>(StringComparer.Ordinal);

        /// <summary>
        /// 置顶偏好的落地文件。Windows 下 Qt 的 QSettings(UserScope) 就是
        /// %APPDATA%\flare\flare-client.ini，这里刻意用同一路径、同一 [sessions] pinned 键，
        /// 将来两个壳切着装，置顶跟着走，不必迁移（Linux 版对应 ~/.config/flare/flare-client.ini）。
        /// </summary>
        public static string PinnedIniPath
        {
            get
            {
                var dir = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "flare");
                return Path.Combine(dir, "flare-client.ini");
            }
        }

        private void LoadPinned()
        {
            _pinned.Clear();
            try
            {
                var path = PinnedIniPath;
                if (!File.Exists(path)) return;
                foreach (var raw in File.ReadAllLines(path, Encoding.UTF8))
                {
                    var line = raw.Trim();
                    var eq = line.IndexOf('=');
                    if (eq <= 0) continue;
                    if (!line.Substring(0, eq).Trim().Equals("pinned", StringComparison.OrdinalIgnoreCase))
                        continue;
                    // QSettings 把 QStringList 写成「a, b, c」一行；顺手剥掉可能带的引号
                    foreach (var part in line.Substring(eq + 1).Split(','))
                    {
                        var id = part.Trim().Trim('"');
                        if (id.Length > 0) _pinned.Add(id);
                    }
                }
            }
            catch (Exception ex)
            {
                Dbg("!! pinned load failed: " + ex.Message);
            }
        }

        private void SavePinned()
        {
            var path = PinnedIniPath;
            var tmp = path + ".tmp";
            try
            {
                var dir = Path.GetDirectoryName(path);
                if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

                // 保留文件里的其它键：这个 INI 将来还会存窗口位置等宿主偏好，
                // 整份覆盖等于把它们一起清了。
                var lines = File.Exists(path)
                    ? new List<string>(File.ReadAllLines(path, Encoding.UTF8))
                    : new List<string>();

                var ids = new List<string>(_pinned);
                ids.Sort(StringComparer.Ordinal);
                var value = "pinned=" + string.Join(", ", ids.ToArray());

                var replaced = false;
                for (var i = 0; i < lines.Count; i++)
                {
                    var t = lines[i].Trim();
                    var eq = t.IndexOf('=');
                    if (eq > 0 && t.Substring(0, eq).Trim().Equals("pinned", StringComparison.OrdinalIgnoreCase))
                    {
                        lines[i] = value;
                        replaced = true;
                        break;
                    }
                }
                if (!replaced)
                {
                    // 没有 [sessions] 段就补一段，别把键塞进别人的段里
                    var hasSection = false;
                    foreach (var l in lines)
                        if (l.Trim().Equals("[sessions]", StringComparison.OrdinalIgnoreCase)) { hasSection = true; break; }
                    if (!hasSection) lines.Add("[sessions]");
                    lines.Add(value);
                }

                // 先写临时文件再原子替换：写到一半崩掉不至于把整份偏好清空
                File.WriteAllLines(tmp, lines.ToArray(), new UTF8Encoding(false));
                try
                {
                    if (File.Exists(path)) File.Replace(tmp, path, null);
                    else File.Move(tmp, path);
                }
                catch
                {
                    File.Copy(tmp, path, true);
                    try { File.Delete(tmp); } catch { }
                }
            }
            catch (Exception ex)
            {
                Dbg("!! pinned save failed: " + ex.Message);
            }
        }

        public bool IsPinned(string id)
        {
            return !string.IsNullOrEmpty(id) && _pinned.Contains(id);
        }

        /// <summary>切换置顶并立刻重排当前列表。与 Qt 一致：只动本地偏好，不通知引擎。</summary>
        public void TogglePin(string id)
        {
            if (string.IsNullOrEmpty(id)) return;
            if (!_pinned.Remove(id)) _pinned.Add(id);
            Dbg((_pinned.Contains(id) ? "pin " : "unpin ") + id);
            SavePinned();
            // 数据一条没变，重新问引擎是白费一轮 IPC：直接拿当前集合重算分组与顺序
            ApplyDaySections(ShowingArchived ? ArchivedSessions : Sessions);
        }

        /// <summary>取消置顶（已是未置顶状态时什么都不做）。软删/彻底删前要先清偏好，
        /// 否则同一 id 以后被复用会莫名带着 📌 复活 —— Qt deleteCurrent/purgeCurrent 都先 removeAll。</summary>
        public void Unpin(string id)
        {
            if (string.IsNullOrEmpty(id)) return;
            if (!_pinned.Remove(id)) return;
            SavePinned();
            ApplyDaySections(ShowingArchived ? ArchivedSessions : Sessions);
        }

        /// <summary>发出搜索。limit=30 与 Qt 一致（sessionlistwidget.cpp:542）。</summary>
        public void SearchSessions(string query, int limit = 30)
        {
            var q = (query ?? "").Trim();
            if (q.Length == 0) return;
            _inflightQuery = q;
            var o = New("search_sessions");
            // 字段名照抄引擎 protocol.go 的 Request：query / limit / archived
            o["query"] = q;
            o["limit"] = limit;
            // 搜索范围跟着视图走：回收站里搜的是归档集合，引擎负责分集合，宿主不拼条件
            o["archived"] = ShowingArchived;
            Send(o);
        }

        /// <summary>清空搜索 → 立刻回到当前视图，不等防抖（Qt :518 同一条）。</summary>
        public void ClearSearch()
        {
            _inflightQuery = "";
            if (ShowingArchived) LoadArchived(); else LoadSessions();
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
            // 置顶偏好先读进来：首次 recent_sessions 回来时 ApplyDaySections 就要用它，
            // 晚一步读会让「📌 置顶」组在启动后的第一屏里缺一条。
            LoadPinned();
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

        /// <summary>
        /// 动作类请求（重命名/归档/清空/恢复/彻底删除）回执后重新拉一次当前视图。
        /// 搜索激活时回来的是搜索视图，清空则回默认视图 —— 与 Qt 「ok 回执 → refresh()」一致。
        /// </summary>
        public void RefreshCurrentView()
        {
            if (SearchQuery.Trim().Length > 0) { SearchSessions(SearchQuery); return; }
            if (ShowingArchived) LoadArchived(); else LoadSessions();
        }
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

        // MARK: - 会话右键动作（口径对齐 Qt 0.6.191 rebuildMenu → 各 *Current 槽）
        // 一律走 compat 层命令：回执形状是 {type:"ok", sessionId, archived/restored/deleted/cleared}，
        // 和 server.go 里的 archive_session/clear_messages（session_updated/messages_cleared）不同名。
        // 宿主只认一种回执，才不会出现「动作成功但列表不刷新」。

        /// <summary>归档到归档箱（软删，可在回收站恢复）。</summary>
        public void EndSession(string id)
        {
            if (string.IsNullOrEmpty(id)) return;
            Unpin(id);
            var o = New("end_session");
            o["sessionId"] = id;
            Send(o);
        }

        /// <summary>从归档箱恢复到最近会话。</summary>
        public void RestoreSession(string id)
        {
            if (string.IsNullOrEmpty(id)) return;
            var o = New("restore_session");
            o["sessionId"] = id;
            Send(o);
        }

        /// <summary>清空该会话的消息，会话本身保留。</summary>
        public void ClearSession(string id)
        {
            if (string.IsNullOrEmpty(id)) return;
            var o = New("clear_session");
            o["sessionId"] = id;
            Send(o);
        }

        /// <summary>彻底删除（不可恢复）。调用方负责先弹确认框。</summary>
        public void PurgeSession(string id)
        {
            if (string.IsNullOrEmpty(id)) return;
            Unpin(id);
            var o = New("delete_session");
            o["sessionId"] = id;
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

                case "ok":
                {
                    // 动作类回执（重命名/归档/恢复/清空/彻底删除）。
                    // compat 层统一回 {type:"ok", sessionId, archived|restored|deleted|cleared}，
                    // 所以这里按出现的键判断动作，再决定要不要动主区、用哪句提示。
                    var sid = (string)obj["sessionId"] ?? "";
                    var isDeleted = obj["deleted"] != null && (bool)obj["deleted"];
                    var isArchived = obj["archived"] != null && (bool)obj["archived"];
                    var isRestored = obj["restored"] != null && (bool)obj["restored"];
                    var isCleared = obj["cleared"] != null;
                    var onCurrent = sid.Length > 0 && sid == SessionID;

                    if (isDeleted)
                    {
                        StatusText = "已彻底删除";
                        if (onCurrent) { SessionID = "default"; Blocks.Clear(); }
                    }
                    else if (isArchived)
                    {
                        StatusText = "已移入回收站，可在「🗑️ 回收站」里恢复";
                        // 归档掉的正好是打开着的会话：留着主区继续显示一个「已进回收站」的
                        // 对话会让人以为还能接着聊，直接让空态接管
                        if (onCurrent) { SessionID = "default"; Blocks.Clear(); }
                    }
                    else if (isRestored) StatusText = "已恢复到最近会话";
                    else if (isCleared)
                    {
                        StatusText = "已清空该会话的消息";
                        if (onCurrent) Blocks.Clear();
                    }
                    else StatusText = "Flare 引擎已连接";

                    Dbg("ok: " + (string)obj["sessionId"] + " " + obj.ToString(Newtonsoft.Json.Formatting.None));
                    // 视图不变，重新拉一遍当前集合（与 Qt 「ok 回执 → refresh()」同一手）
                    RefreshCurrentView();
                    break;
                }

                case "sessions":
                case "recent_sessions":
                    // 先置视图再填：ApplyDaySections 靠 ShowingArchived 判「要不要插分组标题」，
                    // 顺序反了会让回收站里也长出「今天/昨天」，或让主列表反而没有分组。
                    ShowingArchived = false;
                    FillSessions(Sessions, obj);
                    break;

                case "archived_sessions":
                    ShowingArchived = true;
                    FillSessions(ArchivedSessions, obj);
                    break;

                case "search_sessions":
                {
                    // 搜索命中。引擎在回执里回显 query / archived，两个都要校对：
                    // ①快速改关键词时先发的那次可能后到，直接应用会把列表顶成上一个
                    //   关键词的结果，而搜索框写着新词 —— 用户看到「搜的不是我打的字」；
                    // ②关键词相同也可能是跨视图的旧回执（在回收站搜过「灵感」，切回主
                    //   列表再搜「灵感」，先发的归档查询关键词一致），光比 query 挡不住。
                    // 逐条对应 Qt sessionlistwidget.cpp:470-497。
                    var echoed = ((string)obj["query"] ?? "").Trim();
                    var want = SearchQuery.Trim();
                    if (want.Length == 0) break;   // 用户已经把搜索框清空，这批结果没用了
                    var echoedArchived = obj["archived"] != null && obj["archived"].Type != JTokenType.Null
                                         && (bool)obj["archived"];
                    if (echoed.Length > 0 && echoed != want)
                    {
                        Dbg("SEARCH_DROP stale=" + echoed + " current=" + want);
                        break;
                    }
                    if (echoedArchived != ShowingArchived)
                    {
                        Dbg("SEARCH_DROP view echoed_archived=" + echoedArchived + " current=" + ShowingArchived);
                        break;
                    }
                    FillSessions(ShowingArchived ? ArchivedSessions : Sessions, obj, true);
                    break;
                }

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

        private void FillSessions(ObservableCollection<SessionInfo> target, JObject obj, bool isSearch = false)
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
                    // 搜索回执的 preview 是「命中那段文字的上下文」，recent_sessions 的是首条消息。
                    // 两者都是纯事实，这里一视同仁地吃下来即可（不拼措辞）。
                    Preview = (string)it["preview"] ?? "",
                    UpdatedAt = (string)it["updatedAt"] ?? (string)it["updated_at"] ?? "",
                    Count = it["count"] != null ? (int?)it["count"] ?? 0
                         : (it["messageCount"] != null ? (int?)it["messageCount"] ?? 0 : 0),
                    Pinned = IsPinned(id)
                });
            }

            // 截图取证开关：CI 的库里没有历史会话，recent_sessions 恒定回空数组，
            // 于是「单行条目 + 右端时间列 + 超长标题按宽度省略」这套最关键的样式
            // 永远进不了离屏截图 —— 改坏了也拍不出来。FLARE_GUI_DEMO 显式开启才灌样本。
            //
            // 搜索路径也灌，但要**按关键词过滤**：CI 里引擎同样搜不出东西（库是空的），
            // 不灌就永远拍不到「搜索态下没有分组标题、置顶仍排最前」这两条关键行为。
            // 过滤只作用于样本，不影响真实回执；线上环境压根没有 FLARE_GUI_DEMO。
            if (target.Count == 0 && !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("FLARE_GUI_DEMO")))
                InjectDemoSessions(target, isSearch ? SearchQuery : null);

            // 分组放在最后一步：样本注入之后、写进集合之前只此一次机会，
            // 早于注入就会把分组标题和会话项拆成两份对不齐的数据。
            ApplyDaySections(target);
        }

        /// <summary>
        /// 按时间插分组标题行（📌 置顶 / 今天 / 昨天 / 最近 7 天 / 更早）—— 与 Qt
        /// sessionlistwidget.cpp 的 appendSectionHeader 同构：那边也是往同一个
        /// QListWidget 里塞一条 NoItemFlags 的标题项，不是另建一套分组控件。
        /// 分组控件会和虚拟化、滚动、单选互斥三处同时打架，还得再维护两条滚动同步。
        ///
        /// groupByDay 传 null 表示「按当前状态自己判」：搜索激活、或站在回收站视图时
        /// 一律不插标题（Qt sessionlistwidget.cpp:604 同一条）。删空列表后用户再也
        /// 找不到回收站入口那类坑，就是这条规则没抄全埋下的。
        /// </summary>
        private void ApplyDaySections(ObservableCollection<SessionInfo> target, bool? groupByDay = null)
        {
            if (target.Count == 0) return;
            var rows = new List<SessionInfo>();
            foreach (var s in target)
                if (!s.IsHeader) rows.Add(s);
            if (rows.Count == 0) return;

            // 引擎返回理论上已按 updatedAt 倒序；显式再排一次，否则同一分组会被
            // 中间插进来的一条旧会话截断，列表里出现两个「更早」。
            // 置顶项一律排最前（Qt :557 把 pinned 与 normal 分成两段再拼起来）：
            // 置顶的意义就是「永远在顶上」，混在时间序里等于没置顶。
            rows.Sort(delegate (SessionInfo a, SessionInfo b)
            {
                var pa = a.Pinned ? 0 : 1;
                var pb = b.Pinned ? 0 : 1;
                if (pa != pb) return pa.CompareTo(pb);
                var x = a.UpdatedLocal; var y = b.UpdatedLocal;
                if (x == null && y == null) return 0;
                if (x == null) return 1;
                if (y == null) return -1;
                return y.Value.CompareTo(x.Value);
            });

            // 外部重排（置顶切换）后 Pinned 标记要跟着最新的 INI 走，
            // 否则同一次会话内「取消置顶」后图标还挂着。
            foreach (var s in rows) s.Pinned = IsPinned(s.Id);

            var group = groupByDay ?? (SearchQuery.Trim().Length == 0 && !ShowingArchived);

            target.Clear();
            string cur = null;
            foreach (var s in rows)
            {
                if (group)
                {
                    var sec = s.Pinned ? "📌 置顶" : SessionInfo.SectionOf(s.UpdatedLocal);
                    if (sec != cur)
                    {
                        target.Add(new SessionInfo { Id = "", IsHeader = true, HeaderText = sec, Title = sec });
                        cur = sec;
                    }
                }
                target.Add(s);
            }
        }

        /// <summary>
        /// 仅用于离屏截图取证：造几条样本会话，时间跨度覆盖三种时间口径，
        /// 标题长度拉到一定会触发 TextTrimming 的程度。
        /// </summary>
        private void InjectDemoSessions(ObservableCollection<SessionInfo> target, string filter = null)
        {
            var now = DateTime.UtcNow;
            var hasFilter = !string.IsNullOrEmpty(filter);
            // FLARE_GUI_DEMO_PIN=demo-2,demo-5 可让取证截图自带「📌 置顶」组：
            // 置顶是本地 INI 偏好，CI 的 runner 上没有这份文件，不显式指定就永远拍不到。
            var pinEnv = Environment.GetEnvironmentVariable("FLARE_GUI_DEMO_PIN") ?? "";
            foreach (var raw in pinEnv.Split(','))
            {
                var id = raw.Trim();
                if (id.Length > 0) _pinned.Add(id);
            }
            Action<string, string, DateTime> add = (id, title, when) =>
            {
                // 搜索取证：只留标题里真含关键词的样本，模拟引擎的过滤结果
                if (hasFilter && title.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) return;
                target.Add(new SessionInfo
                {
                    Id = id,
                    Title = title,
                    Preview = "",
                    UpdatedAt = when.ToString("yyyy-MM-ddTHH:mm:ss") + "Z",
                    Count = 1,
                    Pinned = _pinned.Contains(id)
                });
            };
            // 今天 → HH:mm；今年 → MM-dd HH:mm；跨年 → yy-MM-dd HH:mm
            // 搜索取证专用样本：FLARE_GUI_SEARCH 按标题过滤，样本里一条都不含关键词时
            // 整片列表会被滤空，「搜索高亮到底画没画出来」这一项就等于没验。
            add("demo-7", "排查 WARP 分流不生效", now.AddHours(-1));
            add("demo-1", "帮我把这段 Go 的并发逻辑改成带超时控制的写法", now.AddMinutes(-8));
            add("demo-2", "灵感稍纵即逝——怎么设计一个不打断思路的速记入口", now.AddHours(-3));
            // 昨天 / 最近 7 天这两组必须各有样本，否则分组标题压根不进画面，
            // 「今天 / 昨天 / 最近 7 天 / 更早」四段只验得到两段。
            add("demo-3", "Windows 原生版和 Qt 版的行为差异清单", now.AddDays(-1).AddHours(-2));
            add("demo-4", "把每次对话的 token 消耗按周做成一张折线图", now.AddDays(-3));
            add("demo-5", "复盘：为什么重影问题在离屏截图里看不出来", now.AddDays(-190));
            add("demo-6", "短标题", now.AddDays(-400));
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
