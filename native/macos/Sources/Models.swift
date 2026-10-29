import Foundation

// Models.swift —— 协议事件映射成的界面数据模型（对应 protocol.md 附录 B）

struct ChatBlock: Identifiable, Equatable {
    enum Role: String {
        case user, flare, tool, system
    }
    let id = UUID()
    var role: Role
    var text: String
    var toolName: String? = nil
    var isError: Bool = false
    var streaming: Bool = false
}

struct SessionInfo: Identifiable, Equatable {
    var id: String
    var title: String
    var updatedAt: String
    var preview: String = ""
    var archived: Bool = false
}

struct MemoryItem: Identifiable, Equatable {
    var id: String
    var content: String
    var createdAt: String = ""
}

struct ToolSpec: Identifiable, Equatable {
    var name: String
    var description: String
    var id: String { name }
}

struct ConfirmRequest: Identifiable, Equatable {
    let id: String          // 引擎发的 confirm.id，回执时必须原样带回
    let sessionId: String
    let toolName: String
    let argsText: String
    let description: String
}
