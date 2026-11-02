// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "FlareMac",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(name: "FlareMac", path: "Sources")
    ]
)
