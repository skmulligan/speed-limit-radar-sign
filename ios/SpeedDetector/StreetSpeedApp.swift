import SwiftUI

@main struct StreetSpeedApp: App {
  @StateObject private var device = DeviceController()
  @Environment(\.scenePhase) private var phase
  var body: some Scene {
    WindowGroup {
      ContentView().environmentObject(device).tint(.mint)
        .onChange(of: phase) { _, value in device.setForeground(value == .active) }
        .alert(
          "Street Speed",
          isPresented: Binding(
            get: { device.errorMessage != nil }, set: { if !$0 { device.errorMessage = nil } })
        ) {
          Button("OK") { device.errorMessage = nil }
        } message: {
          Text(device.errorMessage ?? "")
        }
    }
  }
}
