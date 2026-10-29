import SwiftUI
import AppKit

// Panels.swift —— 四面板，尺寸/模态性与 render-spec §3 一致
// 记忆 420×480 非模态 / 技能 420×480 非模态 / 版本与更新 500×340 非模态 / 设置 440×320 模态

struct PanelHeader: View {
    let title: String
    var body: some View {
        Text(title)
            .font(.system(size: Theme.FS.title, weight: .semibold))
            .foregroundColor(Theme.textMain)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, 12)
            .padding(.top, 12)
            .padding(.bottom, 6)
    }
}

// MARK: - 记忆面板（非模态 420×480）

struct MemoryPanel: View {
    @EnvironmentObject var engine: Engine
    @State private var draft: String = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            PanelHeader(title: "记忆")

            ScrollView {
                LazyVStack(alignment: .leading, spacing: 6) {
                    ForEach(engine.memories) { m in
                        HStack(alignment: .top, spacing: 8) {
                            Text(m.content)
                                .font(.system(size: Theme.FS.base))
                                .foregroundColor(Theme.textMain)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .textSelection(.enabled)
                            Button {
                                engine.deleteMemory(m.id)
                            } label: {
                                Text("删除").font(.system(size: Theme.FS.small)).foregroundColor(Theme.amber)
                            }
                            .buttonStyle(.plain)
                        }
                        .padding(8)
                        .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgSoft))
                        .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))
                    }
                    if engine.memories.isEmpty {
                        Text("还没有记忆条目。")
                            .font(.system(size: Theme.FS.small))
                            .foregroundColor(Theme.textFaint)
                            .padding(.top, 8)
                    }
                }
            }
            .frame(maxHeight: .infinity)

            TextField("新增一条记忆…", text: $draft, axis: .vertical)
                .textFieldStyle(.plain)
                .font(.system(size: Theme.FS.base))
                .lineLimit(2...4)
                .padding(8)
                .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgSoft))
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))

            HStack {
                Button {
                    engine.saveMemory(draft)
                    draft = ""
                } label: {
                    Text("保存记忆")
                        .font(.system(size: Theme.FS.base, weight: .semibold))
                        .foregroundColor(.white)
                        .padding(.horizontal, 14).padding(.vertical, 6)
                        .background(RoundedRectangle(cornerRadius: 6).fill(Theme.orange))
                }
                .buttonStyle(.plain)
                Spacer()
                Text("\(engine.memories.count) 条")
                    .font(.system(size: Theme.FS.small))
                    .foregroundColor(Theme.textMuted)
            }
        }
        .padding(.horizontal, 8)
        .padding(.bottom, 8)
        .frame(minWidth: 420, minHeight: 480)
        .background(Theme.bgWindow)
        .onAppear { engine.loadMemories() }
    }
}

// MARK: - 技能面板（非模态 420×480）

struct SkillPanel: View {
    @EnvironmentObject var engine: Engine

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            PanelHeader(title: "技能与工具")
            Text("引擎当前注册的工具（由引擎上报，壳子不硬编码）")
                .font(.system(size: Theme.FS.small))
                .foregroundColor(Theme.textMuted)
                .padding(.horizontal, 12)

            ScrollView {
                LazyVStack(alignment: .leading, spacing: 6) {
                    ForEach(engine.tools) { t in
                        VStack(alignment: .leading, spacing: 3) {
                            Text(t.name)
                                .font(.system(size: Theme.FS.base, weight: .semibold))
                                .foregroundColor(Theme.textMain)
                            if !t.description.isEmpty {
                                Text(t.description)
                                    .font(.system(size: Theme.FS.small))
                                    .foregroundColor(Theme.textSoft)
                            }
                        }
                        .padding(8)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgSoft))
                        .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))
                    }
                    if engine.tools.isEmpty {
                        Text("引擎尚未上报工具列表。")
                            .font(.system(size: Theme.FS.small))
                            .foregroundColor(Theme.textFaint)
                    }
                }
            }
            .frame(maxHeight: .infinity)

            HStack {
                Button {
                    engine.loadTools()
                } label: {
                    Text("刷新")
                        .font(.system(size: Theme.FS.base))
                        .foregroundColor(Theme.textSoft)
                        .padding(.horizontal, 12).padding(.vertical, 6)
                        .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgSoft))
                        .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))
                }
                .buttonStyle(.plain)
                Spacer()
                Text("\(engine.tools.count) 个")
                    .font(.system(size: Theme.FS.small))
                    .foregroundColor(Theme.textMuted)
            }
        }
        .padding(.horizontal, 12)
        .padding(.bottom, 10)
        .frame(minWidth: 420, minHeight: 480)
        .background(Theme.bgWindow)
        .onAppear { engine.loadTools() }
    }
}

// MARK: - 版本与更新（非模态 500×340）

struct VersionPanel: View {
    @EnvironmentObject var engine: Engine
    @State private var engineLatest: String = ""
    @State private var guiLatest: String = ""
    @State private var note: String = ""

    private static let engineRepo = "raymond202202/flare"
    private static let guiRepo = "raymond202202/flare-client"

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            PanelHeader(title: "版本与更新")

            row("客户端（GUI）", engine.guiVersion, guiLatest, "检查客户端更新") {
                Task { guiLatest = await Self.latest(repo: Self.guiRepo) ?? "查询失败" }
            }
            row("引擎", engine.engineVersion, engineLatest, "检查引擎更新") {
                Task { engineLatest = await Self.latest(repo: Self.engineRepo) ?? "查询失败" }
            }

            Text("引擎路径：\(engine.enginePath.isEmpty ? "未找到" : engine.enginePath)")
                .font(.system(size: Theme.FS.small))
                .foregroundColor(Theme.textMuted)
                .textSelection(.enabled)

            if !note.isEmpty {
                Text(note).font(.system(size: Theme.FS.small)).foregroundColor(Theme.dark)
            }

            Spacer()

            HStack {
                Text("火焰橙 · Flare \(engine.guiVersion)")
                    .font(.system(size: Theme.FS.small))
                    .foregroundColor(Theme.textFaint)
                Spacer()
            }
        }
        .padding(.horizontal, 12)
        .padding(.bottom, 12)
        .frame(minWidth: 500, minHeight: 340)
        .background(Theme.bgWindow)
    }

    private func row(_ label: String, _ current: String, _ latest: String,
                     _ actionTitle: String, _ action: @escaping () -> Void) -> some View {
        HStack(spacing: 10) {
            Text(label)
                .font(.system(size: Theme.FS.base, weight: .semibold))
                .foregroundColor(Theme.textMain)
                .frame(width: 96, alignment: .leading)
            Text("当前 \(current.isEmpty ? "—" : current)")
                .font(.system(size: Theme.FS.base))
                .foregroundColor(Theme.textSoft)
            if !latest.isEmpty {
                Text("最新 \(latest)")
                    .font(.system(size: Theme.FS.small))
                    .foregroundColor(latest == current ? Theme.textMuted : Theme.orange)
            }
            Spacer()
            Button(action: action) {
                Text(actionTitle)
                    .font(.system(size: Theme.FS.small))
                    .foregroundColor(Theme.textSoft)
                    .padding(.horizontal, 10).padding(.vertical, 5)
                    .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgSoft))
                    .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))
            }
            .buttonStyle(.plain)
        }
    }

    static func latest(repo: String) async -> String? {
        guard let url = URL(string: "https://api.github.com/repos/\(repo)/releases/latest") else { return nil }
        var req = URLRequest(url: url)
        req.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        req.timeoutInterval = 12
        do {
            let (data, _) = try await URLSession.shared.data(for: req)
            if let obj = try JSONSerialization.jsonObject(with: data) as? [String: Any],
               let tag = obj["tag_name"] as? String {
                return tag.hasPrefix("v") ? String(tag.dropFirst()) : tag
            }
        } catch {
            return nil
        }
        return nil
    }
}

// MARK: - 设置（模态 440×320）

struct SettingsPanel: View {
    @EnvironmentObject var engine: Engine
    @Environment(\.dismiss) private var dismiss
    @State private var saved = false

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("设置")
                .font(.system(size: Theme.FS.title, weight: .semibold))
                .foregroundColor(Theme.textMain)

            Text("引擎可执行文件")
                .font(.system(size: Theme.FS.small))
                .foregroundColor(Theme.textMuted)
            Text(engine.enginePath.isEmpty ? "未找到（可用环境变量 FLARE_BIN 指定）" : engine.enginePath)
                .font(.system(size: Theme.FS.small, design: .monospaced))
                .foregroundColor(Theme.textMain)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(8)
                .background(RoundedRectangle(cornerRadius: 6).fill(Theme.bgSoft))
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(Theme.borderSoft, lineWidth: 1))

            Text("API 密钥由引擎自己从 ~/.flare/.env 读取，客户端不接触、不保存。")
                .font(.system(size: Theme.FS.small))
                .foregroundColor(Theme.textSoft)
                .fixedSize(horizontal: false, vertical: true)

            HStack {
                Button {
                    engine.stop()
                    engine.start()
                    saved = true
                } label: {
                    Text("重启引擎")
                        .font(.system(size: Theme.FS.base, weight: .semibold))
                        .foregroundColor(.white)
                        .padding(.horizontal, 20).padding(.vertical, 8)
                        .background(RoundedRectangle(cornerRadius: 6).fill(Theme.orange))
                }
                .buttonStyle(.plain)
                Spacer()
                Button("关闭") { dismiss() }
                    .buttonStyle(.plain)
                    .font(.system(size: Theme.FS.base))
                    .foregroundColor(Theme.textSoft)
            }
            Spacer()
            Text(saved ? "已重启引擎" : "客户端版本 \(engine.guiVersion)")
                .font(.system(size: Theme.FS.small))
                .foregroundColor(Theme.textMuted)
        }
        .padding(20)
        .frame(width: 440, height: 320)
        .background(Theme.bgWindow)
    }
}
