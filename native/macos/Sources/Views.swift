import SwiftUI
import AppKit

extension Notification.Name {
    static let flareOpenSettings = Notification.Name("flare.openSettings")
}

// Views.swift —— 界面实现，数值一律取自 render-spec.md

struct ContentView: View {
    @EnvironmentObject var engine: Engine
    @State private var draft: String = ""
    @State private var showSettings = false
    @State private var filter: String = ""
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 0) {
                SidebarView(filter: $filter)
                    .frame(width: 220)
                    .background(Theme.bgSoft)
                Rectangle().fill(Theme.borderSoft).frame(width: 1)
                ChatView(draft: $draft)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .background(Theme.bgWindow)
            }
            StatusBarView(openSettings: { showSettings = true })
        }
        .background(Theme.bgWindow)
        .frame(minWidth: 820, minHeight: 600)
        .onReceive(NotificationCenter.default.publisher(for: .flareOpenSettings)) { _ in
            showSettings = true
        }
        .sheet(isPresented: $showSettings) {
            SettingsPanel()
                .environmentObject(engine)
        }
        .sheet(item: $engine.pendingConfirm) { req in
            ConfirmView(req: req)
                .environmentObject(engine)
        }
        .onReceive(NotificationCenter.default.publisher(for: .flareOpenSettings)) { _ in
            showSettings = true
        }
        .onAppear {
            engine.loadSessions()
        }
    }
}

// MARK: - 侧栏（对齐 render-spec §3.2 与 Qt SessionListWidget 实际布局）

struct SidebarView: View {
    @EnvironmentObject var engine: Engine
    @Binding var filter: String
    @Environment(\.openWindow) private var openWindow
    @State private var hovered: String? = nil
    @State private var renaming: SessionInfo? = nil
    @State private var renameText: String = ""

    private var visible: [SessionInfo] {
        let list = engine.showingArchived ? engine.archivedSessions : engine.sessions
        let f = filter.trimmingCharacters(in: .whitespaces)
        if f.isEmpty { return list }
        return list.filter {
            $0.title.localizedCaseInsensitiveContains(f) || $0.preview.localizedCaseInsensitiveContains(f)
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("会话")
                .font(.system(size: Theme.FS.title, weight: .semibold))
                .foregroundColor(Theme.orange)

            // 搜索框：单行，高 30，圆角 6，底白，边 borderSoft，文字 textGray 13
            HStack(spacing: 4) {
                Text("🔍").font(.system(size: 11))
                TextField("搜索会话…", text: $filter)
                    .textFieldStyle(.plain)
                    .font(.system(size: Theme.FS.base))
                    .foregroundColor(Theme.textGray)
                if !filter.isEmpty {
                    Button { filter = "" } label: {
                        Image(systemName: "xmark.circle.fill")
                            .font(.system(size: 11))
                            .foregroundColor(Theme.textFaint)
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 6)
            .frame(height: 30)
            .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgWindow))
            .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))

            // 会话列表：弹性占满；底 bgSoft、边 borderSoft、圆角 8、内边距 4
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 1) {
                    ForEach(visible) { s in
                        SessionRow(session: s,
                                   selected: s.id == engine.sessionID,
                                   hovered: hovered == s.id)
                            .contentShape(Rectangle())
                            .onHover { inside in
                                if inside { hovered = s.id }
                                else if hovered == s.id { hovered = nil }
                            }
                            .onTapGesture { engine.switchSession(s.id) }
                            .contextMenu {
                                Button("重命名…") {
                                    renameText = s.title
                                    renaming = s
                                }
                                Button("删除", role: .destructive) { engine.deleteSession(s.id) }
                            }
                    }
                    if visible.isEmpty {
                        Text(engine.showingArchived ? "暂无归档会话" : "暂无会话")
                            .font(.system(size: Theme.FS.small))
                            .foregroundColor(Theme.textFaint)
                            .frame(maxWidth: .infinity, alignment: .center)
                            .padding(.top, 12)
                    }
                }
                .padding(4)
            }
            .frame(maxHeight: .infinity)
            .background(RoundedRectangle(cornerRadius: 8).fill(Theme.bgSoft))
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(Theme.borderSoft, lineWidth: 1))

            // 按钮行 1：＋ 新建（主）｜删除（次）
            HStack(spacing: 6) {
                SideButton(title: "＋ 新建", fg: .white, bg: Theme.orange,
                           hoverBg: Color(hex: 0xe0630f), border: Color.clear) {
                    engine.newSession()
                }
                SideButton(title: "删除", fg: Theme.orange, bg: Theme.bgWindow,
                           hoverBg: Theme.hoverSoft, border: Theme.borderSoft) {
                    engine.deleteSession(engine.sessionID)
                }
            }
            // 归档 / 记忆 / 技能 / 设置：Qt 侧逐个 addWidget，各占整行
            SideButton(title: "📦 归档", fg: Theme.amber, bg: Theme.bgWindow,
                       hoverBg: Theme.hoverSoft, border: Theme.borderSoft) {
                if engine.showingArchived { engine.loadSessions() } else { engine.loadArchived() }
            }
            SideButton(title: "🧠 记忆", fg: Theme.orange, bg: Theme.bgWindow,
                       hoverBg: Theme.hoverSoft, border: Theme.borderSoft) {
                openWindow(id: "memories")
            }
            SideButton(title: "🔧 技能", fg: Theme.orange, bg: Theme.bgWindow,
                       hoverBg: Theme.hoverSoft, border: Theme.borderSoft) {
                openWindow(id: "skills")
            }
            SideButton(title: "⚙️ 设置", fg: Theme.orange, bg: Theme.bgWindow,
                       hoverBg: Theme.hoverSoft, border: Theme.borderSoft) {
                NotificationCenter.default.post(name: .flareOpenSettings, object: nil)
            }
        }
        .padding(8)
        .frame(width: 220)
        .sheet(item: $renaming) { s in
            VStack(alignment: .leading, spacing: 10) {
                Text("重命名会话")
                    .font(.system(size: Theme.FS.title, weight: .semibold))
                    .foregroundColor(Theme.orange)
                TextField("会话标题", text: $renameText)
                    .textFieldStyle(.plain)
                    .font(.system(size: Theme.FS.body))
                    .padding(.horizontal, 10)
                    .padding(.vertical, 8)
                    .background(RoundedRectangle(cornerRadius: 8).fill(Theme.bgWindow))
                    .overlay(RoundedRectangle(cornerRadius: 8).stroke(Theme.borderSoft, lineWidth: 1))
                HStack(spacing: 8) {
                    Spacer()
                    Button("取消") { renaming = nil }
                        .buttonStyle(.plain)
                        .font(.system(size: Theme.FS.base))
                        .foregroundColor(Theme.textSoft)
                    Button {
                        let t = renameText.trimmingCharacters(in: .whitespacesAndNewlines)
                        if !t.isEmpty { engine.renameSession(s.id, title: t) }
                        renaming = nil
                    } label: {
                        Text("保存")
                            .font(.system(size: Theme.FS.base))
                            .foregroundColor(.white)
                            .padding(.horizontal, 16)
                            .padding(.vertical, 6)
                            .background(RoundedRectangle(cornerRadius: 6).fill(Theme.orange))
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(16)
            .frame(width: 340)
            .background(Theme.bgSoft)
        }
    }
}

/// 侧栏按钮：主/次两种配色，悬停换底色（对应 Qt 的 `:hover` 规则）
struct SideButton: View {
    let title: String
    let fg: Color
    let bg: Color
    let hoverBg: Color
    let border: Color
    let action: () -> Void
    @State private var hovering = false

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: Theme.FS.base))
                .foregroundColor(fg)
                .frame(maxWidth: .infinity)
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .background(RoundedRectangle(cornerRadius: 6).fill(hovering ? hoverBg : bg))
                .overlay(RoundedRectangle(cornerRadius: 6)
                    .stroke(border, lineWidth: border == Color.clear ? 0 : 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .onHover { hovering = $0 }
    }
}

/// 会话列表项：双行（标题 + 预览 · 时间），悬停 hoverSoft、选中 orange 底白字
struct SessionRow: View {
    let session: SessionInfo
    let selected: Bool
    let hovered: Bool

    /// 与 Qt sessionlistwidget.cpp 一致的拼接规则：预览（截 24 字）在前，时间在后；无预览则只显示时间
    private var subtitle: String {
        var second = ""
        if !session.preview.isEmpty {
            let raw = session.preview.trimmingCharacters(in: .whitespacesAndNewlines)
            second = raw.count > 24 ? String(raw.prefix(24)) + "…" : raw
        }
        let t = shortTime(session.updatedAt)
        if !t.isEmpty { second += (second.isEmpty ? "" : "  ·  ") + t }
        return second
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(session.title.isEmpty ? "新会话" : session.title)
                .font(.system(size: Theme.FS.base, weight: selected ? .semibold : .regular))
                .foregroundColor(selected ? Color.white : Theme.textMain)
                .lineLimit(1)
            if !subtitle.isEmpty {
                Text(subtitle)
                    .font(.system(size: Theme.FS.faint))
                    .foregroundColor(selected ? Color.white.opacity(0.85) : Theme.textMuted)
                    .lineLimit(1)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.horizontal, 6)
        .padding(.vertical, 8)
        .background(RoundedRectangle(cornerRadius: 6)
            .fill(selected ? Theme.orange : (hovered ? Theme.hoverSoft : Color.clear)))
    }
}

/// Qt `shortTime` 对齐：今天 HH:mm，其余 MM-dd；解析失败退化为前 10 字符
func shortTime(_ iso: String) -> String {
    guard !iso.isEmpty else { return "" }
    let cleaned = iso.replacingOccurrences(of: "Z", with: "")
    let f = DateFormatter()
    f.locale = Locale(identifier: "en_US_POSIX")
    f.timeZone = TimeZone.current
    var parsed: Date? = nil
    for fmt in ["yyyy-MM-dd'T'HH:mm:ss.SSS", "yyyy-MM-dd'T'HH:mm:ss",
                "yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd"] {
        f.dateFormat = fmt
        if let d = f.date(from: cleaned) { parsed = d; break }
    }
    guard let d = parsed else { return String(iso.prefix(10)) }
    let out = DateFormatter()
    out.locale = Locale(identifier: "en_US_POSIX")
    out.dateFormat = Calendar.current.isDateInToday(d) ? "HH:mm" : "MM-dd"
    return out.string(from: d)
}

// MARK: - 聊天区

struct ChatView: View {
    @EnvironmentObject var engine: Engine
    @Binding var draft: String

    var body: some View {
        VStack(spacing: 8) {
            Text("Flare 对话")
                .font(.system(size: Theme.FS.chatTitle, weight: .semibold))
                .foregroundColor(Theme.orange)
                .frame(maxWidth: .infinity, alignment: .leading)

            // 输出区：弹性占满；底 bgSoft、边 borderSoft、圆角 8、内边距 8、字号 14
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 10) {
                        if engine.blocks.isEmpty {
                            WelcomeView()
                        }
                        ForEach(engine.blocks) { b in
                            BlockView(block: b).id(b.id)
                        }
                        Color.clear.frame(height: 1).id("bottom")
                    }
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                .background(RoundedRectangle(cornerRadius: 8).fill(Theme.bgSoft))
                .overlay(RoundedRectangle(cornerRadius: 8).stroke(Theme.borderSoft, lineWidth: 1))
                .onChange(of: engine.blocks.count) { _ in
                    withAnimation(.easeOut(duration: 0.12)) { proxy.scrollTo("bottom", anchor: .bottom) }
                }
                .onChange(of: engine.blocks.last?.text) { _ in
                    proxy.scrollTo("bottom", anchor: .bottom)
                }
            }
            .frame(maxHeight: .infinity)

            // 输入行：输入框白底圆角 8、内边距 8×10；发送主按钮 8×18；停止白底红字红边 8×14
            HStack(spacing: 8) {
                TextField("输入消息，Enter 发送", text: $draft, axis: .vertical)
                    .textFieldStyle(.plain)
                    .font(.system(size: Theme.FS.body))
                    .lineLimit(1...6)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 8)
                    .background(RoundedRectangle(cornerRadius: 8).fill(Theme.bgWindow))
                    .overlay(RoundedRectangle(cornerRadius: 8).stroke(Theme.borderSoft, lineWidth: 1))
                    .submitLabel(.send)
                    .onSubmit { submit() }

                Button { submit() } label: {
                    Text("发送")
                        .font(.system(size: Theme.FS.body))
                        .foregroundColor(.white)
                        .padding(.horizontal, 18)
                        .padding(.vertical, 8)
                        .background(RoundedRectangle(cornerRadius: 8).fill(Theme.orange))
                }
                .buttonStyle(.plain)
                .keyboardShortcut(.return, modifiers: [])
                .disabled(engine.busy)

                if engine.busy {
                    Button { engine.cancel() } label: {
                        Text("⏹ 停止")
                            .font(.system(size: Theme.FS.body))
                            .foregroundColor(Theme.dangerText)
                            .padding(.horizontal, 14)
                            .padding(.vertical, 8)
                            .background(RoundedRectangle(cornerRadius: 8).fill(Theme.bgWindow))
                            .overlay(RoundedRectangle(cornerRadius: 8)
                                .stroke(Theme.dangerBorder, lineWidth: 1))
                    }
                    .buttonStyle(.plain)
                }
            }
        }
        .padding(12)
        .frame(minWidth: 480, maxWidth: .infinity, maxHeight: .infinity)
    }

    private func submit() {
        let t = draft
        guard !t.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
        draft = ""
        engine.sendChat(t)
    }
}

struct WelcomeView: View {
    @State private var phase: Double = 0
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            FlameText(text: "Flare", size: Theme.FS.hero, weight: .bold, phase: phase)
            Text("问点什么，或者把活儿丢给我。")
                .font(.system(size: Theme.FS.title))
                .foregroundColor(Theme.textSoft)
            Text("灵感之旅 · 从一句话开始")
                .font(.system(size: Theme.FS.small))
                .foregroundColor(Theme.dark)
        }
        .padding(.top, 40)
        .onAppear {
            withAnimation(.linear(duration: 5).repeatForever(autoreverses: false)) { phase = 5 }
        }
    }
}

struct BlockView: View {
    let block: ChatBlock

    var body: some View {
        switch block.role {
        case .user:
            HStack(alignment: .top, spacing: 0) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("你").font(.system(size: Theme.FS.small, weight: .bold)).foregroundColor(Theme.orange)
                    bodyText(block.text)
                }
                .padding(10)
                .background(RoundedRectangle(cornerRadius: 8).fill(Theme.bgAlt))
                Spacer(minLength: 40)
            }
        case .flare:
            // 助手消息也给气泡（白底 + 细边），否则与用户的暖黄气泡不对称，视觉重心偏向左上
            HStack(alignment: .top, spacing: 0) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Flare").font(.system(size: Theme.FS.small, weight: .bold)).foregroundColor(Theme.orange)
                    bodyText(block.text)
                    if block.streaming {
                        Text("…").font(.system(size: Theme.FS.small)).foregroundColor(Theme.textFaint)
                    }
                }
                .padding(10)
                .background(RoundedRectangle(cornerRadius: 8).fill(Theme.bgWindow))
                .overlay(RoundedRectangle(cornerRadius: 8).stroke(Theme.borderSoft, lineWidth: 1))
                Spacer(minLength: 40)
            }
        case .tool:
            VStack(alignment: .leading, spacing: 4) {
                Text(block.toolName ?? "工具")
                    .font(.system(size: Theme.FS.small, weight: .semibold))
                    .foregroundColor(block.isError ? Theme.dangerText : Theme.dark)
                Text(block.text)
                    .font(.system(size: Theme.FS.small, design: .monospaced))
                    .foregroundColor(block.isError ? Theme.dangerText : Theme.textSoft)
                    .textSelection(.enabled)
            }
            .padding(8)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgAlt))
            .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))
        case .system:
            Text(block.text)
                .font(.system(size: Theme.FS.small))
                .foregroundColor(Theme.textMuted)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    @ViewBuilder
    private func bodyText(_ s: String) -> some View {
        if let attr = try? AttributedString(markdown: s) {
            Text(attr)
                .font(.system(size: Theme.FS.body))
                .foregroundColor(Theme.textMain)
                .textSelection(.enabled)
        } else {
            Text(s)
                .font(.system(size: Theme.FS.body))
                .foregroundColor(Theme.textMain)
                .textSelection(.enabled)
        }
    }
}

// MARK: - 确认弹窗

struct ConfirmView: View {
    @EnvironmentObject var engine: Engine
    let req: ConfirmRequest
    @State private var alternative: String = ""
    @State private var showAlternative = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("工具调用需要确认")
                .font(.system(size: Theme.FS.title, weight: .semibold))
                .foregroundColor(Theme.textMain)
            Text(req.description.isEmpty ? "引擎请求执行工具「\(req.toolName)」" : req.description)
                .font(.system(size: Theme.FS.base))
                .foregroundColor(Theme.textSoft)
            ScrollView {
                Text(req.argsText)
                    .font(.system(size: Theme.FS.small, design: .monospaced))
                    .foregroundColor(Theme.textMain)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .textSelection(.enabled)
            }
            .frame(height: 140)
            .padding(8)
            .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgSoft))
            .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))

            if showAlternative {
                TextField("告诉引擎改怎么做（替代方案）", text: $alternative)
                    .textFieldStyle(.roundedBorder)
                    .font(.system(size: Theme.FS.base))
            }

            HStack(spacing: 8) {
                confirmButton("允许一次", primary: true) { engine.answerConfirm("allow_once") }
                confirmButton("本会话允许", primary: false) { engine.answerConfirm("allow_session") }
                confirmButton("永久允许", primary: false) { engine.answerConfirm("always") }
                if showAlternative {
                    confirmButton("提交替代", primary: false) {
                        engine.answerConfirm("alternative", alternative: alternative)
                    }
                } else {
                    confirmButton("替代做法") { showAlternative = true }
                }
                Spacer()
                Button { engine.answerConfirm("deny") } label: {
                    Text("拒绝")
                        .font(.system(size: Theme.FS.base, weight: .semibold))
                        .foregroundColor(Theme.dangerText)
                        .padding(.horizontal, 14).padding(.vertical, 7)
                        .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgSoft))
                        .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.dangerBorder, lineWidth: 1))
                }
                .buttonStyle(.plain)
            }
        }
        .padding(20)
        .frame(width: 460)
        .background(Theme.bgWindow)
    }

    private func confirmButton(_ title: String, primary: Bool = false, _ action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: Theme.FS.base, weight: .semibold))
                .foregroundColor(primary ? .white : Theme.textSoft)
                .padding(.horizontal, 14).padding(.vertical, 7)
                .background(RoundedRectangle(cornerRadius: 6).fill(primary ? Theme.orange : Theme.bgSoft))
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(primary ? Color.clear : Theme.borderSoft, lineWidth: 1))
        }
        .buttonStyle(.plain)
    }
}

// MARK: - 状态栏（render-spec §3.1：左消息；右 模型 + 版本链接，两个版本号来源不同不得合并）

struct StatusBarView: View {
    @EnvironmentObject var engine: Engine
    var openSettings: () -> Void
    @Environment(\.openWindow) private var openWindow

    private var modelText: String {
        if engine.modelName.isEmpty { return "🤖 模型加载中…" }
        return engine.providerName.isEmpty
            ? "🤖 \(engine.modelName)"
            : "🤖 \(engine.modelName) · \(engine.providerName)"
    }

    private var versionText: String {
        let kernel = engine.engineVersion.isEmpty ? "—" : engine.engineVersion
        return "客户端 v\(engine.clientVersion) · 内核 flare v\(kernel)"
    }

    var body: some View {
        HStack(spacing: 8) {
            Text(engine.statusText.isEmpty ? "Flare 引擎已连接" : engine.statusText)
                .font(.system(size: Theme.FS.small))
                .foregroundColor(engine.busy ? Theme.orange : Theme.textSoft)
            Spacer(minLength: 8)
            Text(modelText)
                .font(.system(size: Theme.FS.small))
                .foregroundColor(Theme.orange)
                .padding(.horizontal, 8)
            Button { openWindow(id: "version") } label: {
                Text(versionText)
                    .font(.system(size: Theme.FS.small))
                    .foregroundColor(Theme.dark)
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 8)
        .frame(height: 26)
        .background(Theme.bgSoft)
        .overlay(Rectangle().fill(Theme.borderSoft).frame(height: 1), alignment: .top)
    }
}
