import Foundation
import Combine
import SwiftUI

// Engine.swift —— 引擎子进程桥。壳子只做两件事：画界面 + 收发 JSON Lines。
// 铁律（render-spec §0）：UI 不读 ~/.flare/.env、不内嵌引擎逻辑，一律 `flare server` 子进程。

@MainActor
final class Engine: ObservableObject {
    enum Status: String {
        case stopped = "引擎未启动"
        case starting = "正在启动引擎…"
        case ready = "就绪"
    }

    @Published var status: Status = .stopped
    @Published var blocks: [ChatBlock] = []
    @Published var sessions: [SessionInfo] = []
    @Published var memories: [MemoryItem] = []
    @Published var tools: [ToolSpec] = []
    @Published var pendingConfirm: ConfirmRequest? = nil
    @Published var busy: Bool = false
    @Published var statusText: String = "未启动"
    @Published var engineVersion: String = "—"
    @Published var sessionID: String = "default"
    @Published var archivedSessions: [SessionInfo] = []
    @Published var showingArchived: Bool = false
    @Published var modelName: String = ""
    @Published var providerName: String = ""
    @Published var usageText: String = ""
    @Published var lastError: String? = nil
    @Published var enginePath: String = ""

    /// 客户端版本：来自 Info.plist 的 CFBundleShortVersionString（编译期常量），
    /// 与运行时 `version` 帧的内核版本是两个不同来源，状态栏不得合并成一个数字。
    var clientVersion: String {
        (Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String) ?? "0.0.0"
    }

    let guiVersion = (Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String) ?? "0.0.0"

    private var proc: Process?
    private var stdinPipe: Pipe?
    private var stdoutPipe: Pipe?
    private var stderrPipe: Pipe?
    private var lineBuffer = Data()
    private var stderrTail: [String] = []
    private var cancelRequested = false
    /// 引擎回过 cancelled 帧即置 true（截图/自测用）
    @Published var cancelledOnce = false

    // 调试日志：仅当 FLARE_GUI_LOG 环境变量指向可写文件时启用（诊断用，正常用户不产生任何文件）
    private static let logURL: URL? = {
        guard let p = ProcessInfo.processInfo.environment["FLARE_GUI_LOG"], !p.isEmpty else { return nil }
        return URL(fileURLWithPath: p)
    }()

    nonisolated private static func dbg(_ s: String) {
        guard let url = logURL else { return }
        let line = String(format: "%.3f ", Date().timeIntervalSince1970) + s + "\n"
        if let h = try? FileHandle(forWritingTo: url) {
            h.seekToEndOfFile(); h.write(Data(line.utf8)); try? h.close()
        } else {
            try? line.write(to: url, atomically: true, encoding: .utf8)
        }
    }

    // MARK: - 生命周期

    func start() {
        guard proc == nil else { return }
        guard let exe = Self.locateEngine() else {
            status = .stopped
            statusText = "找不到 flare 引擎可执行文件"
            appendSystem("未找到 flare 引擎。请把引擎放到应用包内 Contents/Resources/bin/flare，或设置环境变量 FLARE_BIN。")
            return
        }
        enginePath = exe.path
        Self.dbg("== spawn \(exe.path) server")
        let p = Process()
        p.executableURL = exe
        p.arguments = ["server"]

        let inP = Pipe(), outP = Pipe(), errP = Pipe()
        p.standardInput = inP
        p.standardOutput = outP
        p.standardError = errP

        outP.fileHandleForReading.readabilityHandler = { [weak self] h in
            let d = h.availableData
            guard !d.isEmpty else { return }
            Task { @MainActor in self?.feed(d) }
        }
        errP.fileHandleForReading.readabilityHandler = { [weak self] h in
            let d = h.availableData
            guard !d.isEmpty, let s = String(data: d, encoding: .utf8) else { return }
            Task { @MainActor in
                guard let self else { return }
                for line in s.split(separator: "\n") {
                    self.stderrTail.append(String(line))
                }
                if self.stderrTail.count > 40 { self.stderrTail.removeFirst(self.stderrTail.count - 40) }
            }
        }
        p.terminationHandler = { [weak self] proc in
            Task { @MainActor in
                guard let self else { return }
                self.status = .stopped
                self.statusText = "引擎已退出（code \(proc.terminationStatus)）"
                self.busy = false
                self.proc = nil
            }
        }

        do {
            try p.run()
        } catch {
            status = .stopped
            statusText = "引擎启动失败：\(error.localizedDescription)"
            appendSystem(statusText)
            return
        }
        proc = p
        stdinPipe = inP
        stdoutPipe = outP
        stderrPipe = errP
        status = .starting
        statusText = "正在启动引擎…"

        send(["type": "version"])
        // 侧栏首屏用 recent_sessions 而不是 list_sessions：只有它带 preview 字段
        // （internal/host/protocol.go 的 RecentSessionInfo，Preview 取首条用户消息），
        // 会话项才能像 Qt 版一样展示「标题 + 预览 · 时间」两行。
        send(["type": "recent_sessions"])
        send(["type": "tools"])
        // 模型名/提供商由引擎的 models 帧给出（internal/host/compat2.go 的 configured 字段），
        // 不在本地配置里推断，保证状态栏显示的就是引擎真正生效的模型。
        send(["type": "models"])

        // 自测钩子：FLARE_GUI_SMOKE="问题" 时，启动 3 秒后自动发一条聊天，用于无图形权限环境下的端到端验证
        if let smoke = ProcessInfo.processInfo.environment["FLARE_GUI_SMOKE"], !smoke.isEmpty {
            Self.dbg("== smoke enabled")
            Task { @MainActor [weak self] in
                try? await Task.sleep(nanoseconds: 3_000_000_000)
                self?.sendChat(smoke)
                // FLARE_GUI_AUTOCANCEL=秒：发出后 N 秒点「停止」，用于验证 cancel 全链路
                if let cs = ProcessInfo.processInfo.environment["FLARE_GUI_AUTOCANCEL"],
                   let secs = Double(cs), secs > 0 {
                    try? await Task.sleep(nanoseconds: UInt64(secs * 1_000_000_000))
                    self?.cancel()
                }
            }
        }
    }

    func stop() {
        if let p = proc, p.isRunning {
            send(["type": "shutdown"])
            let deadline = Date().addingTimeInterval(1.5)
            while p.isRunning && Date() < deadline {
                usleep(50_000)
            }
            if p.isRunning { p.terminate() }
        }
        proc = nil
        stdinPipe = nil
        status = .stopped
    }

    // MARK: - 发送

    func send(_ obj: [String: Any]) {
        guard let pipe = stdinPipe, let p = proc, p.isRunning else { return }
        guard let data = try? JSONSerialization.data(withJSONObject: obj),
              var line = String(data: data, encoding: .utf8) else { return }
        line += "\n"
        Self.dbg(">> " + line.trimmingCharacters(in: .whitespacesAndNewlines))
        pipe.fileHandleForWriting.write(Data(line.utf8))
    }

    func sendChat(_ text: String) {
        let t = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty, !busy else { return }
        blocks.append(ChatBlock(role: .user, text: t))
        blocks.append(ChatBlock(role: .flare, text: "", streaming: true))
        busy = true
        cancelRequested = false
        statusText = "生成中…"
        send(["type": "chat", "input": t, "sessionId": sessionID])
    }

    func cancel() {
        guard busy else { return }
        cancelRequested = true
        statusText = "正在停止…"
        send(["type": "cancel", "sessionId": sessionID])
    }

    func answerConfirm(_ decision: String, alternative: String = "") {
        guard let c = pendingConfirm else { return }
        var obj: [String: Any] = ["type": "confirm_response", "id": c.id, "decision": decision]
        if !alternative.isEmpty { obj["alternative"] = alternative }
        send(obj)
        pendingConfirm = nil
    }

    func loadSessions() { showingArchived = false; send(["type": "recent_sessions"]) }
    func loadArchived() { showingArchived = true; send(["type": "archived_sessions"]) }
    func loadModels() { send(["type": "models"]) }
    func loadMemories() { send(["type": "list_memories"]) }
    func loadTools() { send(["type": "tools"]) }
    func loadUsage() { send(["type": "get_usage"]) }
    func loadMessages() { send(["type": "get_messages", "sessionId": sessionID]) }

    func newSession() {
        send(["type": "new_session"])
    }

    func switchSession(_ id: String) {
        guard id != sessionID else { return }
        sessionID = id
        blocks.removeAll()
        send(["type": "get_messages", "sessionId": id])
    }

    func saveMemory(_ content: String) {
        let t = content.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { return }
        send(["type": "save_memory", "content": t])
        send(["type": "list_memories"])
    }

    func deleteMemory(_ id: String) {
        send(["type": "delete_memory", "id": id])
        send(["type": "list_memories"])
    }

    func renameSession(_ id: String, title: String) {
        send(["type": "update_session_title", "sessionId": id, "title": title])
    }

    func deleteSession(_ id: String) {
        send(["type": "delete_session", "sessionId": id])
        if id == sessionID { sessionID = "default"; blocks.removeAll() }
        send(["type": "recent_sessions"])
    }

    // MARK: - 接收

    private func feed(_ data: Data) {
        lineBuffer.append(data)
        while let idx = lineBuffer.firstIndex(of: 0x0a) {
            let lineData = lineBuffer[lineBuffer.startIndex..<idx]
            lineBuffer.removeSubrange(lineBuffer.startIndex...idx)
            guard let line = String(data: lineData, encoding: .utf8) else { continue }
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            if trimmed.isEmpty { continue }
            handle(line: trimmed)
        }
    }

    private func handle(line: String) {
        Self.dbg("<< " + line)
        guard let data = line.data(using: .utf8),
              let obj = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any],
              let type = obj["type"] as? String else {
            return
        }
        switch type {
        case "text":
            let chunk = (obj["content"] as? String) ?? (obj["text"] as? String) ?? ""
            appendToStreamingFlare(chunk)

        case "done":
            finishStreaming()
            Self.dbg("done: blocks=\(blocks.count)")
            if let s = obj["sessionId"] as? String, !s.isEmpty { sessionID = s }
            busy = false
            statusText = "Flare 引擎已连接"
            send(["type": "recent_sessions"])
            if cancelRequested {
                appendSystem("⏹ 已停止生成")
                cancelRequested = false
            }

        case "tool_call":
            let name = (obj["toolName"] as? String) ?? "tool"
            let args = (obj["args"] as? String) ?? ""
            blocks.append(ChatBlock(role: .tool, text: args, toolName: name))
            finishStreaming()

        case "tool_result":
            let name = (obj["toolName"] as? String) ?? "tool"
            let out = (obj["content"] as? String) ?? ""
            let isErr = (obj["isErr"] as? Bool) ?? false
            blocks.append(ChatBlock(role: .tool, text: out, toolName: "\(name) 结果", isError: isErr))

        case "confirm":
            let id = (obj["id"] as? String) ?? (obj["confirmId"] as? String) ?? ""
            let name = (obj["name"] as? String) ?? "tool"
            var argsText = ""
            if let a = obj["args"] { argsText = Self.pretty(a) }
            pendingConfirm = ConfirmRequest(
                id: id,
                sessionId: (obj["sessionId"] as? String) ?? sessionID,
                toolName: name,
                argsText: argsText,
                description: (obj["description"] as? String) ?? ""
            )

        case "confirm_result":
            if let allow = obj["allow"] as? Bool {
                appendSystem(allow ? "✅ 已放行：\(obj["confirmId"] as? String ?? "")" : "⛔ 已拒绝本次工具调用")
            }

        case "cancelled":
            finishStreaming()
            busy = false
            statusText = "已停止"
            // 提示在这里给过了，done 的兜底分支不要再重复一遍
            cancelRequested = false
            cancelledOnce = true
            appendSystem("⏹ Flare：已停止生成")
            send(["type": "recent_sessions"])

        case "error":
            finishStreaming()
            busy = false
            let msg = (obj["message"] as? String) ?? "引擎返回未知错误"
            lastError = msg
            statusText = "错误"
            appendSystem("⚠️ \(msg)")

        case "sessions":
            sessions = Self.parseSessions(obj)

        case "session_created":
            if let id = obj["sessionId"] as? String { sessionID = id }
            blocks.removeAll()
            send(["type": "recent_sessions"])

        case "session_updated":
            send(["type": "recent_sessions"])

        case "messages":
            blocks = Self.parseMessages(obj)

        case "memories":
            memories = Self.parseMemories(obj)

        case "tools":
            tools = Self.parseTools(obj)

        case "version":
            if let v = (obj["protocol"] as? String) {
                _ = v
            }
            engineVersion = (obj["engine"] as? String) ?? (obj["version"] as? String) ?? "—"
            status = .ready
            statusText = "Flare 引擎已连接"

        case "usage":
            usageText = Self.parseUsage(obj)

        case "recent_sessions":
            // 与 sessions 同构（id/title/updatedAt/preview），只是多了预览、少了 archive 标记
            sessions = Self.parseSessions(obj)
            showingArchived = false

        case "archived_sessions":
            archivedSessions = Self.parseSessions(obj)
            showingArchived = true

        case "models":
            if let cfg = obj["configured"] as? [String: Any],
               let main = cfg["main"] as? [String: Any] {
                modelName = (main["model"] as? String) ?? ""
                providerName = (main["provider"] as? String) ?? ""
            }
            if status != .ready { status = .ready }
            // models 帧到齐＝引擎已就绪；状态栏文案统一在这里定稿，避免停在中间态（如「就绪」）
            statusText = "Flare 引擎已连接"

        case "pong", "ok", "confirm_status", "messages_cleared", "memory_deleted":
            if type == "messages_cleared" { blocks.removeAll() }
            if type == "memory_deleted" { send(["type": "list_memories"]) }

        default:
            // 向前兼容铁律：未知 type 静默忽略，不得崩、不得报错
            break
        }
    }

    // MARK: - 流式块

    private func appendToStreamingFlare(_ chunk: String) {
        guard !chunk.isEmpty else { return }
        if let last = blocks.last, last.role == .flare, last.streaming {
            blocks[blocks.count - 1].text += chunk
        } else {
            blocks.append(ChatBlock(role: .flare, text: chunk, streaming: true))
        }
    }

    private func finishStreaming() {
        for i in blocks.indices where blocks[i].streaming {
            blocks[i].streaming = false
        }
    }

    private func appendSystem(_ text: String) {
        blocks.append(ChatBlock(role: .system, text: text))
    }

    // MARK: - 解析辅助

    private static func pretty(_ any: Any) -> String {
        if let s = any as? String { return s }
        if let d = try? JSONSerialization.data(withJSONObject: any, options: [.prettyPrinted, .sortedKeys]),
           let s = String(data: d, encoding: .utf8) {
            return s
        }
        return "\(any)"
    }

    private static func parseSessions(_ obj: [String: Any]) -> [SessionInfo] {
        let raw = (obj["sessions"] as? [[String: Any]]) ?? []
        return raw.compactMap { d in
            let id = (d["id"] as? String) ?? (d["sessionId"] as? String) ?? ""
            guard !id.isEmpty else { return nil }
            let title = (d["title"] as? String) ?? (d["name"] as? String) ?? "未命名会话"
            let upd = (d["updatedAt"] as? String) ?? (d["updated_at"] as? String) ?? ""
            let prev = (d["preview"] as? String) ?? ""
            return SessionInfo(id: id, title: title.isEmpty ? "未命名会话" : title, updatedAt: upd, preview: prev)
        }
    }

    private static func parseMemories(_ obj: [String: Any]) -> [MemoryItem] {
        let raw = (obj["memories"] as? [[String: Any]]) ?? (obj["items"] as? [[String: Any]]) ?? []
        return raw.compactMap { d in
            let id = (d["id"] as? String) ?? ""
            let content = (d["content"] as? String) ?? (d["text"] as? String) ?? ""
            guard !id.isEmpty || !content.isEmpty else { return nil }
            return MemoryItem(id: id, content: content, createdAt: (d["createdAt"] as? String) ?? "")
        }
    }

    private static func parseTools(_ obj: [String: Any]) -> [ToolSpec] {
        let raw = (obj["tools"] as? [[String: Any]]) ?? []
        return raw.compactMap { d in
            let name = (d["name"] as? String) ?? ""
            guard !name.isEmpty else { return nil }
            return ToolSpec(name: name, description: (d["description"] as? String) ?? "")
        }
    }

    private static func parseUsage(_ obj: [String: Any]) -> String {
        if let st = obj["stats"] as? [String: Any] {
            let total = st["totalTokens"] as? Int ?? 0
            let reqs = st["requestCount"] as? Int ?? 0
            return "累计 \(reqs) 次请求 / \(total) tokens"
        }
        return ""
    }

    private static func parseMessages(_ obj: [String: Any]) -> [ChatBlock] {
        let raw = (obj["messages"] as? [[String: Any]]) ?? []
        var out: [ChatBlock] = []
        for m in raw {
            let role = (m["role"] as? String) ?? ""
            let content = (m["content"] as? String) ?? ""
            switch role {
            case "user":
                out.append(ChatBlock(role: .user, text: content))
            case "assistant":
                if !content.isEmpty { out.append(ChatBlock(role: .flare, text: content)) }
            case "tool":
                out.append(ChatBlock(role: .tool, text: content, toolName: (m["name"] as? String) ?? "tool"))
            default:
                break
            }
        }
        return out
    }

    // MARK: - 引擎定位

    static func locateEngine() -> URL? {
        let fm = FileManager.default
        if let ov = ProcessInfo.processInfo.environment["FLARE_BIN"], fm.isExecutableFile(atPath: ov) {
            return URL(fileURLWithPath: ov)
        }
        var cands: [String] = []
        // 包内引擎固定放 Contents/Resources/bin/flare。
        // 绝不能放 Contents/MacOS/flare —— APFS 默认大小写不敏感，会盖掉 GUI 主程序 Flare。
        if let res = Bundle.main.resourceURL {
            cands.append(res.appendingPathComponent("bin/flare").path)
        }
        if let exeDir = Bundle.main.executableURL?.deletingLastPathComponent() {
            cands.append(exeDir.appendingPathComponent("../Resources/bin/flare").path)
        }
        cands.append(NSHomeDirectory() + "/.flare/install/flare")
        cands.append("/usr/local/bin/flare")
        cands.append("/opt/homebrew/bin/flare")
        if let path = ProcessInfo.processInfo.environment["PATH"] {
            for d in path.split(separator: ":") { cands.append("\(d)/flare") }
        }
        for c in cands where fm.isExecutableFile(atPath: c) {
            return URL(fileURLWithPath: c)
        }
        return nil
    }
}
