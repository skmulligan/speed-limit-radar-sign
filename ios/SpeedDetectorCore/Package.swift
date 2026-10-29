// swift-tools-version: 5.9
import PackageDescription
let package = Package(name: "SpeedDetectorCore", platforms: [.iOS(.v17), .macOS(.v13), .watchOS(.v10)], products: [.library(name: "SpeedDetectorCore", targets: ["SpeedDetectorCore"])], targets: [
    .systemLibrary(name: "CSQLite", pkgConfig: "sqlite3"),
    .target(name: "SpeedDetectorCore", dependencies: ["CSQLite"]),
    .testTarget(name: "SpeedDetectorCoreTests", dependencies: ["SpeedDetectorCore"], resources: [.copy("Fixtures")])
])
