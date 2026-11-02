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
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 0) {
                SidebarView()
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
    @Environment(\.openWindow) private var openWindow
    @State private var hovered: String? = nil
    @State private var renaming: SessionInfo? = nil
    @State private var renameText: String = ""
    @State private var purgeTarget: SessionInfo? = nil

    private var isSearching: Bool {
        !engine.searchQuery.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    /// 数据一律来自引擎：搜索态用 search_sessions 回执，其余按当前视图取。
    /// 这里不再做本地二次过滤——引擎是 LIKE 匹配，本地再滤一遍只会让两边口径越走越远
    /// （Qt 侧同样只认引擎回执）。
    private var source: [SessionInfo] {
        if isSearching { return engine.searchHits }
        return engine.showingArchived ? engine.archivedSessions : engine.sessions
    }

    /// 空会话不占列表：引擎建库就写 title='新会话'，无真标题也无首条消息预览，
    /// 点开是空的纯噪音（Qt sessionlistwidget.cpp:618 同口径）。
    private var visible: [SessionInfo] {
        source.filter { !(FlareTime.isPlaceholderTitle($0.title) && $0.preview.isEmpty) }
    }

    /// 分组：搜索态与回收站平铺；最近视图 = 置顶 → 今天 → 昨天 → 最近 7 天 → 更早
    private var groups: [(key: String, rows: [SessionInfo])] {
        if isSearching || engine.showingArchived { return [("", visible)] }
        var pinRows: [SessionInfo] = []
        var buckets: [String: [SessionInfo]] = [:]
        for s in visible {
            if engine.isPinned(s.id) {
                pinRows.append(s)
            } else {
                buckets[FlareTime.section(s.updatedAt), default: []].append(s)
            }
        }
        var out: [(key: String, rows: [SessionInfo])] = []
        if !pinRows.isEmpty { out.append(("📌 置顶", pinRows)) }
        for key in ["今天", "昨天", "最近 7 天", "更早"] {
            if let rows = buckets[key], !rows.isEmpty { out.append((key, rows)) }
        }
        return out
    }

    /// 主文本：真标题优先；占位标题拿首条消息预览顶上（Qt M8 口径）。
    /// 截断交给 SwiftUI 的 lineLimit，数据层不动字符。
    private func displayTitle(_ s: SessionInfo) -> String {
        let t = s.title.trimmingCharacters(in: .whitespacesAndNewlines)
        var main = t
        if FlareTime.isPlaceholderTitle(t) {
            let p = s.preview.trimmingCharacters(in: .whitespacesAndNewlines)
            main = p.isEmpty ? "新会话" : p.replacingOccurrences(of: "\n", with: " ")
        }
        if engine.isPinned(s.id) { main = "📌 " + main }
        return main
    }

    private func openRename(_ s: SessionInfo) {
        renameText = FlareTime.isPlaceholderTitle(s.title) ? "" : s.title
        renaming = s
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 6) {
                Text(engine.showingArchived ? "📦 归档箱" : "会话")
                    .font(.system(size: Theme.FS.title, weight: .semibold))
                    .foregroundColor(Theme.orange)
                Spacer()
                if engine.showingArchived {
                    Button("返回") { engine.loadSessions(); engine.runSearch("") }
                        .buttonStyle(.plain)
                        .font(.system(size: Theme.FS.faint))
                        .foregroundColor(Theme.textSoft)
                }
            }

            // 搜索框：单行，高 30，圆角 6，底白，边 borderSoft，文字 textGray 13
            HStack(spacing: 4) {
                Text("🔍").font(.system(size: 11))
                // 单一数据源：输入框直接读写 engine.searchQuery。
                // 之前绑 ContentView 的本地 @State filter，导致自动化/外部调用 runSearch 时
                // 输入框与列表不同步（截图里搜索框空、列表没过滤）。
                TextField("搜索会话…", text: Binding(
                    get: { engine.searchQuery },
                    set: { engine.runSearch($0) }))
                    .textFieldStyle(.plain)
                    .font(.system(size: Theme.FS.base))
                    .foregroundColor(Theme.textGray)
                if !engine.searchQuery.isEmpty {
                    Button { engine.runSearch("") } label: {
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
                    ForEach(groups, id: \.key) { group in
                        if !group.key.isEmpty && group.key != "📌 置顶" {
                            Text(group.key)
                                .font(.system(size: Theme.FS.faint, weight: .medium))
                                .foregroundColor(Theme.textFaint)
                                .padding(.horizontal, 6)
                                .padding(.top, 6)
                                .padding(.bottom, 1)
                        } else if group.key == "📌 置顶" {
                            Text(group.key)
                                .font(.system(size: Theme.FS.faint, weight: .medium))
                                .foregroundColor(Theme.amber)
                                .padding(.horizontal, 6)
                                .padding(.top, 6)
                                .padding(.bottom, 1)
                        }
                        ForEach(group.rows) { s in
                            SessionRow(title: displayTitle(s),
                                       preview: FlareTime.isPlaceholderTitle(s.title) ? "" : s.preview,
                                       keyword: isSearching ? engine.searchQuery : "",
                                       stamp: FlareTime.listStampCompact(s.updatedAt),
                                       full: FlareTime.fullStamp(s.updatedAt),
                                       selected: s.id == engine.sessionID,
                                       hovered: hovered == s.id)
                                .contentShape(Rectangle())
                                .onHover { inside in
                                    if inside { hovered = s.id }
                                    else if hovered == s.id { hovered = nil }
                                }
                                .onTapGesture { engine.switchSession(s.id) }
                                .contextMenu { rowMenu(s) }
                        }
                    }
                    if visible.isEmpty {
                        Text(emptyHint)
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
                    if engine.showingArchived { purgeTarget = currentRow() }
                    else { engine.archiveSession(engine.sessionID) }
                }
            }
            // 归档 / 记忆 / 技能 / 设置：Qt 侧逐个 addWidget，各占整行
            SideButton(title: engine.showingArchived ? "↩︎ 返回最近会话" : "📦 归档箱",
                       fg: Theme.amber, bg: Theme.bgWindow,
                       hoverBg: Theme.hoverSoft, border: Theme.borderSoft) {
                if engine.showingArchived { engine.loadSessions() } else { engine.loadArchived() }
                engine.runSearch("")
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
        .sheet(item: $renaming) { s in renameSheet(s) }
        .alert("彻底删除会话？", isPresented: Binding(
            get: { purgeTarget != nil },
            set: { if !$0 { purgeTarget = nil } }
        ), presenting: purgeTarget) { s in
            Button("取消", role: .cancel) { purgeTarget = nil }
            Button("彻底删除", role: .destructive) {
                engine.deleteSession(s.id)
                purgeTarget = nil
            }
        } message: { s in
            Text("「\(displayTitle(s))」的消息会从引擎里真删掉，不进回收站，无法恢复。")
        }
    }

    private var emptyHint: String {
        if isSearching { return "没有匹配的会话" }
        return engine.showingArchived ? "回收站是空的" : "还没有会话，点「＋ 新建」开始"
    }

    /// 「删除」按钮在回收站视图＝对当前选中会话彻底删除
    private func currentRow() -> SessionInfo? {
        visible.first { $0.id == engine.sessionID }
    }

    @ViewBuilder
    private func rowMenu(_ s: SessionInfo) -> some View {
        Button("重命名…") { openRename(s) }
        Button(engine.isPinned(s.id) ? "取消置顶" : "置顶") { engine.togglePin(s.id) }
        Divider()
        if engine.showingArchived {
            Button("恢复会话") { engine.restoreSession(s.id) }
            Button("清空消息") { engine.clearMessages(s.id) }
            Divider()
            Button("彻底删除…", role: .destructive) { purgeTarget = s }
        } else {
            Button("移入回收站") { engine.archiveSession(s.id) }
        }
    }

    @ViewBuilder
    private func renameSheet(_ s: SessionInfo) -> some View {
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
/// 侧栏行：主文本（标题或首条预览）+ 副行（预览）+ 右侧时间列。
/// 时间单独一列右对齐，不拼进副行字符串——Qt 侧就是这么画的
/// （searchhighlightdelegate.cpp:134，area 右边让 12pt 给时间列）。
struct SessionRow: View {
    let title: String
    let preview: String
    /// 搜索命中词：非空时标题/预览里的命中段铺琥珀底（对齐 Qt SearchHighlightDelegate）
    var keyword: String = ""
    let stamp: String
    let full: String
    let selected: Bool
    let hovered: Bool

    var body: some View {
        HStack(alignment: .bottom, spacing: 6) {
            VStack(alignment: .leading, spacing: 1) {
                Text(highlighted(title, keyword: keyword, selected: selected))
                    .font(.system(size: Theme.FS.base, weight: selected ? .semibold : .regular))
                    .foregroundColor(selected ? .white : Theme.textMain)
                    .lineLimit(1)
                    .truncationMode(.tail)
                if !preview.isEmpty {
                    Text(highlighted(preview.replacingOccurrences(of: "\n", with: " "), keyword: keyword, selected: selected))
                        .font(.system(size: Theme.FS.faint))
                        .foregroundColor(selected ? Color.white.opacity(0.85) : Theme.textMuted)
                        .lineLimit(1)
                        .truncationMode(.tail)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Text(stamp)
                .font(.system(size: Theme.FS.faint))
                .foregroundColor(selected ? Color.white.opacity(0.8) : Theme.textSoft)
                .lineLimit(1)
                .fixedSize()
        }
        .padding(.horizontal, 6)
        .padding(.vertical, 6)
        .background(RoundedRectangle(cornerRadius: 6)
            .fill(selected ? Theme.orange : (hovered ? Theme.hoverSoft : Color.clear)))
        .help(full.isEmpty ? title : full)
    }
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
                        // 底部留白 14pt：滚到底时最后一条气泡不贴滚动区下边框（此前 1pt 会显得被切断）
                        Color.clear.frame(height: 14).id("bottom")
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

// MARK: - 搜索高亮

/// 把 keyword 的每一处命中铺琥珀底色 + 暗琥珀字色，其余段落正常。
/// 逐字对齐 Qt `SearchHighlightDelegate`：keyword 先 trim、大小写不敏感、
/// 命中底色 Theme.yellow(#fbbf24)、字色 Theme.dark(#b45309)；
/// 选中行不铺底（底色已是主题橙，铺琥珀会糊成一团，Qt 侧同样只描字不铺底）。
func highlighted(_ text: String, keyword: String, selected: Bool = false) -> AttributedString {
    var attr = AttributedString(text)
    let kw = keyword.trimmingCharacters(in: .whitespacesAndNewlines)
    if kw.isEmpty { return attr }
    // 逐字对齐 Qt SearchHighlightDelegate：底色 yellow(#fbbf24)，选中行 alpha 165、常态 140；
    // 命中字色统一 dark(#b45309)。Qt 在选中态同样高亮，这里不能因为 selected 就跳过。
    let bg = Theme.yellow.opacity(selected ? 165.0 / 255.0 : 140.0 / 255.0)
    var from = attr.startIndex
    while from < attr.endIndex,
          let r = attr[from...].range(of: kw, options: .caseInsensitive) {
        attr[r].backgroundColor = bg
        attr[r].foregroundColor = Theme.dark
        from = r.upperBound
        if from >= attr.endIndex { break }
    }
    return attr
}
