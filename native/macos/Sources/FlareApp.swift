import SwiftUI
import AppKit

// FlareApp.swift —— 应用入口
// 铁律①：无菜单栏（Qt 版 setMenuBar(nullptr)，macOS 侧 NSApp.mainMenu = nil）
// 铁律②：引擎是子进程，壳子只画界面
// 铁律③：强制浅色，不跟随系统深色

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.mainMenu = nil
        NSApp.appearance = NSAppearance(named: .aqua)
        NSApp.windows.forEach { $0.titlebarAppearsTransparent = false }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }
}

@main
struct FlareApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject private var engine = Engine()

    var body: some Scene {
        WindowGroup("Flare") {
            ContentView()
                .environmentObject(engine)
                .preferredColorScheme(.light)
                .task {
                    if engine.status == .stopped { engine.start() }
                    ShotKit.runIfRequested(engine: engine)
                }
        }
        .defaultSize(width: 1024, height: 700)
        .windowResizability(.contentMinSize)
        .commands {
            CommandGroup(replacing: .newItem) {}
            CommandGroup(replacing: .appInfo) {}
            CommandGroup(replacing: .appSettings) {}
        }

        Window("记忆", id: "memories") {
            MemoryPanel()
                .environmentObject(engine)
                .preferredColorScheme(.light)
        }
        .defaultSize(width: 420, height: 480)
        .windowResizability(.contentMinSize)

        Window("技能与工具", id: "skills") {
            SkillPanel()
                .environmentObject(engine)
                .preferredColorScheme(.light)
        }
        .defaultSize(width: 420, height: 480)
        .windowResizability(.contentMinSize)

        Window("版本与更新", id: "version") {
            VersionPanel()
                .environmentObject(engine)
                .preferredColorScheme(.light)
        }
        .defaultSize(width: 500, height: 340)
        .windowResizability(.contentMinSize)
    }
}

// MARK: - 自截图（仅供无「屏幕录制」权限的自动化验证；普通用户永不触发）
// 原理：用 NSView.cacheDisplay 渲染自己窗口的图层到 PNG，不经过 WindowServer 采集，
// 因此不需要 TCC 屏幕录制授权。设置 FLARE_GUI_SHOT=/输出目录 启用。
enum ShotKit {
    static func runIfRequested(engine: Engine) {
        guard let dir = ProcessInfo.processInfo.environment["FLARE_GUI_SHOT"], !dir.isEmpty else { return }
        let out = URL(fileURLWithPath: dir, isDirectory: true)
        try? FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)

        // 生成中截图：从启动就轮询 busy 首次为 true 时抓图。
        // 不能放在后面面板截图的回调里——那已是启动 7s 之后，短问题早答完了。
        Task { @MainActor in
            let t0 = Date()
            for _ in 0..<240 {
                // elapsed >= 6s 才可能拿到真实图层（前面的都是未提交的启动态）
                if engine.busy, Date().timeIntervalSince(t0) >= 6,
                   let main = NSApp.windows.first(where: { $0.isVisible && $0.contentView != nil }) {
                    // 再等 1.5s：SSH 会话下没有 vsync，SwiftUI 提交一帧常要 1s 以上，
                    // 0.4s 抓到的是上一帧（实测与启动态字节完全相同）
                    try? await Task.sleep(nanoseconds: 1_500_000_000)
                    save(main, out, "06-busy")
                    break
                }
                try? await Task.sleep(nanoseconds: 250_000_000)
            }
        }

        // 停止链路截图：等引擎真的回 cancelled 再抓一张
        Task { @MainActor in
            for _ in 0..<600 {
                if engine.cancelledOnce, let main = NSApp.windows.first(where: { $0.isVisible && $0.contentView != nil }) {
                    try? await Task.sleep(nanoseconds: 1_500_000_000)
                    save(main, out, "07-cancelled")
                    break
                }
                try? await Task.sleep(nanoseconds: 100_000_000)
            }
        }

        DispatchQueue.main.asyncAfter(deadline: .now() + 4) {
            if let main = NSApp.windows.first(where: { $0.isVisible && $0.contentView != nil }) {
                save(main, out, "01-main")
            }

            let specs: [(String, AnyView, CGFloat, CGFloat)] = [
                ("02-memories", AnyView(MemoryPanel().environmentObject(engine).preferredColorScheme(.light)), 420, 480),
                ("03-skills",   AnyView(SkillPanel().environmentObject(engine).preferredColorScheme(.light)), 420, 480),
                ("04-version",  AnyView(VersionPanel().environmentObject(engine).preferredColorScheme(.light)), 500, 340),
                ("05-settings", AnyView(SettingsPanel().environmentObject(engine).preferredColorScheme(.light)), 440, 320),
            ]
            var wins: [NSWindow] = []
            for (_, view, w, h) in specs {
                let win = NSWindow(contentRect: NSRect(x: 60, y: 60, width: w, height: h),
                                   styleMask: [.titled, .closable], backing: .buffered, defer: false)
                win.appearance = NSAppearance(named: .aqua)
                let host = NSHostingView(rootView: view)
                host.frame = NSRect(x: 0, y: 0, width: w, height: h)
                win.contentView = host
                win.setContentSize(NSSize(width: w, height: h))
                win.makeKeyAndOrderFront(nil)
                wins.append(win)
            }

            DispatchQueue.main.asyncAfter(deadline: .now() + 3) {
                for (i, win) in wins.enumerated() { save(win, out, specs[i].0) }

                // 等冒烟对话真正结束（助手文本已到且 busy 落回）再抓主窗口终态。
                // 固定延时不可靠：引擎响应耗时随问题长度变化（实测 5s～20s）。
                Task { @MainActor in
                    var waited = 0
                    while true {
                        let hasReply = engine.blocks.contains { $0.role == .flare && !$0.text.isEmpty }
                        if hasReply && !engine.busy { break }
                        if engine.blocks.isEmpty && waited >= 8 { break }
                        if waited >= 60 { break }
                        try? await Task.sleep(nanoseconds: 1_000_000_000)
                        waited += 1
                    }
                    if let main = NSApp.windows.first(where: { $0.isVisible && $0.contentView != nil }) {
                        save(main, out, "01b-main-after")
                    }
                    if ProcessInfo.processInfo.environment["FLARE_GUI_SHOT_QUIT"] != nil {
                        // 留时间给 06/07 收尾：Go 引擎的 cancel 要等 HTTP 流的分块边界才生效，
                        // 实测最长约 21s 才回 cancelled 帧
                        let grace: UInt64 = (ProcessInfo.processInfo.environment["FLARE_GUI_AUTOCANCEL"] != nil)
                            ? 30_000_000_000 : 5_000_000_000
                        try? await Task.sleep(nanoseconds: grace)
                        NSApp.terminate(nil)
                    }
                }
            }
        }
    }

    private static func save(_ window: NSWindow, _ dir: URL, _ name: String) {
        guard let cv = window.contentView, cv.bounds.width > 1, cv.bounds.height > 1 else {
            print("shot: \(name) 无有效 contentView")
            return
        }
        // 强制布局 + 强制重绘：只 layout 不 display 会抓到上一帧的陈旧图层
        // （实测 06-busy 与 01-main 字节完全相同，即根本没重绘）
        window.displayIfNeeded()
        cv.needsDisplay = true
        cv.layoutSubtreeIfNeeded()
        cv.displayIfNeeded()
        CATransaction.begin()
        CATransaction.flush()
        CATransaction.commit()
        RunLoop.current.run(until: Date().addingTimeInterval(0.2))
        guard let rep = cv.bitmapImageRepForCachingDisplay(in: cv.bounds) else { return }
        cv.cacheDisplay(in: cv.bounds, to: rep)
        guard let png = rep.representation(using: .png, properties: [:]) else { return }
        let url = dir.appendingPathComponent("\(name).png")
        try? png.write(to: url)
        print("shot: \(url.path) \(Int(rep.pixelsWide))x\(Int(rep.pixelsHigh))")
    }
}
