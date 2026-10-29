import Charts
import SpeedDetectorCore
import SwiftUI

struct ContentView: View {
  var body: some View {
    TabView {
      NavigationStack { RadarView() }.tabItem {
        Label("Radar", systemImage: "dot.radiowaves.left.and.right")
      }
      NavigationStack { HistoryView() }.tabItem {
        Label("History", systemImage: "clock.arrow.circlepath")
      }
      NavigationStack { TrendsView() }.tabItem { Label("Trends", systemImage: "chart.bar.xaxis") }
      NavigationStack { DeviceView() }.tabItem {
        Label("Device", systemImage: "antenna.radiowaves.left.and.right")
      }
    }
  }
}
struct RadarView: View {
  @EnvironmentObject private var device: DeviceController
  private var targets: [[Int]] { device.fresh ? device.snapshot?.targets ?? [] : [] }
  private var selected: Int { device.snapshot?.selected ?? -1 }
  private var outsideForwardView: Int {
    targets.filter { $0.count == 5 && !(-90...90).contains($0[0]) }.count
  }
  var body: some View {
    ScrollView {
      VStack(spacing: 20) {
        HStack {
          Label(device.liveLabel, systemImage: device.fresh ? "circle.fill" : "circle")
            .foregroundStyle(device.fresh ? .mint : .secondary)
          Spacer()
          if let age = device.ageMS { Text("\(Int(age)) ms").monospacedDigit().font(.caption) }
        }
        VStack(spacing: 0) {
          Text(
            targets.indices.contains(selected)
              ? String(format: "%.0f", Double(targets[selected][3]) / 1.609344) : "—"
          )
          .font(.system(size: 88, weight: .semibold, design: .rounded)).monospacedDigit()
          .foregroundStyle(device.fresh && device.snapshot?.warning == true ? .orange : .primary)
          .accessibilityLabel(
            targets.indices.contains(selected)
              ? "\(Int((Double(targets[selected][3])/1.609344).rounded())) miles per hour"
              : "No current speed")
          Text("MPH").font(.headline).foregroundStyle(.secondary)
          if device.fresh && device.snapshot?.warning == true {
            Text("SLOW DOWN").font(.headline).foregroundStyle(.orange).padding(.top, 8)
          }
        }
        RadarPlot(targets: targets, selected: selected).aspectRatio(1.5, contentMode: .fit)
          .background(Color.mint.opacity(0.04), in: RoundedRectangle(cornerRadius: 24))
          .accessibilityLabel(
            "Forward radar plot. Nominal cone plus or minus 20 degrees. \(targets.count) current detections.")
        Text(
          "Nominal cone ±20° · 25 m rings\nSensor-relative position · angle orientation and direction remain provisional."
        ).font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.center)
        if outsideForwardView > 0 {
          Text("\(outsideForwardView) outside forward view · see detection details below.")
            .font(.caption).foregroundStyle(.secondary)
        }
        ForEach(Array(targets.enumerated()), id: \.offset) { i, t in
          HStack {
            Image(systemName: i == selected ? "scope" : "circle").foregroundStyle(
              i == selected ? .mint : .secondary)
            VStack(alignment: .leading) {
              Text("Detection \(i+1)\(i==selected ? " · sign selected":"")")
              Text("\(t[1]) m · \(t[0])° · direction raw \(t[2]) · SNR raw \(t[4])").font(.caption)
                .foregroundStyle(.secondary)
            }
            Spacer()
            Text(String(format: "%.1f mph", Double(t[3]) / 1.609344)).monospacedDigit()
          }.padding(12).background(.quaternary, in: RoundedRectangle(cornerRadius: 12))
        }
        if targets.isEmpty {
          Text("Fresh detections appear here while connected. Saved episodes are in History.")
            .foregroundStyle(.secondary).multilineTextAlignment(.center)
        }
      }.padding()
    }.navigationTitle("Live radar")
  }
}
struct RadarPlot: View {
  @Environment(\.colorScheme) private var colorScheme
  let targets: [[Int]], selected: Int
  var body: some View {
    Canvas { context, size in
      let center = CGPoint(x: size.width / 2, y: size.height - 36)
      let radius = max(0, min(size.width / 2 - 22, size.height - 72))
      guard radius > 0 else { return }
      func arc(_ radius: CGFloat, from start: Double = -180, to end: Double = 0) -> Path {
        var path = Path()
        path.addArc(
          center: center, radius: radius, startAngle: .degrees(start),
          endAngle: .degrees(end), clockwise: false)
        return path
      }
      func sector(from start: Double, to end: Double) -> Path {
        var path = Path()
        path.move(to: center)
        path.addArc(
          center: center, radius: radius, startAngle: .degrees(start),
          endAngle: .degrees(end), clockwise: false)
        path.closeSubpath()
        return path
      }
      // User Manual V1.00, p. 14: nominal ±20°; a visual guide, not a target filter.
      // Reports beyond the forward hemisphere remain in the detection details.
      context.drawLayer { field in
        field.clip(to: sector(from: -180, to: 0))
        field.drawLayer { shading in
          shading.addFilter(.blur(radius: 7))
          shading.fill(sector(from: -110, to: -70), with: .color(.mint.opacity(0.12)))
          let shadow = Color.black.opacity(colorScheme == .dark ? 0.45 : 0.12)
          shading.fill(sector(from: -180, to: -110), with: .color(shadow))
          shading.fill(sector(from: -70, to: 0), with: .color(shadow))
        }
      }
      for fraction in [0.25, 0.5, 0.75, 1.0] {
        let r = radius * fraction
        context.stroke(arc(r), with: .color(.mint.opacity(0.25)), lineWidth: 1)
        context.draw(
          Text("\(Int(fraction * 100))\(fraction == 1 ? " m" : "")")
            .font(.system(size: 11)).foregroundStyle(.secondary),
          at: CGPoint(x: center.x - r * 0.91, y: center.y - r * 0.41 - 7))
      }
      var baseline = Path()
      baseline.move(to: CGPoint(x: center.x - radius, y: center.y))
      baseline.addLine(to: CGPoint(x: center.x + radius, y: center.y))
      context.stroke(baseline, with: .color(.mint.opacity(0.25)), lineWidth: 1)
      var forward = Path()
      forward.move(to: center)
      forward.addLine(to: CGPoint(x: center.x, y: center.y - radius))
      context.stroke(
        forward, with: .color(.mint.opacity(0.2)), style: StrokeStyle(lineWidth: 1, dash: [4, 4]))
      context.fill(
        Path(ellipseIn: CGRect(x: center.x - 4, y: center.y - 4, width: 8, height: 8)),
        with: .color(.secondary))
      context.draw(
        Text("FORWARD").font(.system(size: 11, weight: .medium)).foregroundStyle(.secondary),
        at: CGPoint(x: center.x, y: center.y - radius - 18))
      context.draw(
        Text("RADAR").font(.system(size: 11, weight: .medium)).foregroundStyle(.secondary),
        at: CGPoint(x: center.x, y: center.y + 22))
      for (i, t) in targets.enumerated() where t.count == 5 && (-90...90).contains(t[0]) {
        let angle = Double(t[0]) * Double.pi / 180
        let d = Double(t[1]) / 100 * radius
        let point = CGPoint(x: center.x + sin(angle) * d, y: center.y - cos(angle) * d)
        let r: CGFloat = i == selected ? 7 : 4
        if i == selected {
          context.fill(
            Path(ellipseIn: CGRect(x: point.x - 12, y: point.y - 12, width: 24, height: 24)),
            with: .color(.mint.opacity(0.15)))
        }
        context.fill(
          Path(ellipseIn: CGRect(x: point.x - r, y: point.y - r, width: 2 * r, height: 2 * r)),
          with: .color(i == selected ? .mint : .orange))
        context.draw(Text("\(i+1)").font(.caption2), at: CGPoint(x: point.x + 12, y: point.y - 10))
      }
    }
  }
}
struct HistoryView: View {
  @EnvironmentObject private var device: DeviceController
  @State private var speedingOnly = false
  @State private var limitDate = false
  @State private var date = Date()
  @State private var session = "all"
  @State private var exportURLs: [URL] = []
  @State private var showShare = false
  private var sessions: [String] {
    Array(Set(device.episodes.map { $0.device + "/" + $0.boot })).sorted()
  }
  private var filtered: [DetectionEpisodeSummary] {
    device.episodes.filter { e in
      (!speedingOnly || e.speeding) && (session == "all" || session == e.device + "/" + e.boot)
        && (!limitDate || e.date.map { Calendar.current.isDate($0, inSameDayAs: date) } == true)
    }
  }
  var body: some View {
    List {
      Section {
        Toggle("Above 20 mph", isOn: $speedingOnly)
        Toggle("Filter by day", isOn: $limitDate)
        if limitDate { DatePicker("Day", selection: $date, displayedComponents: .date) }
        Picker("Session", selection: $session) {
          Text("All sessions").tag("all")
          ForEach(sessions, id: \.self) { Text(String($0.suffix(8))).tag($0) }
        }
      }
      Section("\(filtered.count) detection episodes") {
        ForEach(filtered) { episode in
          NavigationLink {
            EpisodeView(episode: episode)
          } label: {
            HStack {
              VStack(alignment: .leading, spacing: 4) {
                if let date = episode.date {
                  Text(date, format: .dateTime.month().day().hour().minute().second())
                } else {
                  Text("Unknown calendar time")
                }
                Text(
                  "\(episode.duration,specifier:"%.1f") s · \(episode.samples) samples · \(episode.timeLabel)"
                ).font(.caption).foregroundStyle(.secondary)
              }
              Spacer()
              VStack(alignment: .trailing) {
                Text("\(episode.peakMPH,specifier:"%.0f") mph").font(.headline).foregroundStyle(
                  episode.speeding ? .orange : .primary)
                Text("peak").font(.caption).foregroundStyle(.secondary)
              }
            }
          }
        }
        if filtered.isEmpty {
          ContentUnavailableView(
            "No episodes", systemImage: "clock",
            description: Text("Connect to the sign and tap Sync now. History stays on this phone."))
        }
      }
      Section {
        Text(
          "Episodes are detection activity, not verified vehicle counts. Unknown dates are excluded from calendar trends."
        ).font(.caption).foregroundStyle(.secondary)
      }
    }.navigationTitle("History")
      .toolbar {
        Button("Export", systemImage: "square.and.arrow.up") { Task { await export() } }.disabled(
          device.episodes.isEmpty)
      }
      .sheet(isPresented: $showShare) { ShareFiles(urls: exportURLs) }
  }
  private func export() async {
    guard let store = device.store else { return }
    do {
      let csv = try await store.exportCSV()
      let directory = FileManager.default.temporaryDirectory.appendingPathComponent(
        UUID().uuidString, isDirectory: true)
      try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
      let summary = directory.appendingPathComponent("episodes.csv")
      let buckets = directory.appendingPathComponent("speed-buckets.csv")
      try csv.episodes.write(to: summary, atomically: true, encoding: .utf8)
      try csv.buckets.write(to: buckets, atomically: true, encoding: .utf8)
      exportURLs = [summary, buckets]
      showShare = true
    } catch { device.errorMessage = error.localizedDescription }
  }
}
struct EpisodeView: View {
  @EnvironmentObject private var device: DeviceController
  let episode: DetectionEpisodeSummary
  @State private var buckets: [SpeedBucket] = []
  var body: some View {
    List {
      Section {
        LabeledContent("Peak", value: String(format: "%.1f mph", episode.peakMPH))
        LabeledContent("Sample-weighted mean", value: String(format: "%.1f mph", episode.meanMPH))
        LabeledContent("Duration", value: String(format: "%.2f s", episode.duration))
        LabeledContent("Timestamp quality", value: episode.timeLabel)
        LabeledContent("Status", value: episode.status)
        LabeledContent("Maximum simultaneous detections", value: String(episode.targets))
      }
      Section("Speed vector · one-second buckets") {
        Chart(buckets) { b in
          let x = Double(b.mono - episode.startMono) / 1000
          RuleMark(
            x: .value("Elapsed seconds", x),
            yStart: .value("Minimum MPH", Double(b.minimum) / 1.609344),
            yEnd: .value("Maximum MPH", Double(b.maximum) / 1.609344)
          ).foregroundStyle(.mint.opacity(0.4)).lineStyle(StrokeStyle(lineWidth: 5))
          PointMark(x: .value("Elapsed seconds", x), y: .value("Mean MPH", b.meanMPH))
            .foregroundStyle(.mint)
        }.frame(height: 220).chartXAxisLabel("Seconds from first detection").chartYAxisLabel("MPH")
        Text(
          "Dots show sample-weighted means; bars show minimum–maximum. A bucket may contain only part of a second."
        ).font(.caption).foregroundStyle(.secondary)
      }
      Section {
        Text(
          "The sign selects the fastest eligible detection in each report. Selection can move between targets within an episode."
        ).font(.caption)
      }
    }.navigationTitle("Detection episode").task {
      do { buckets = try await device.store?.buckets(episode) ?? [] } catch {
        device.errorMessage = error.localizedDescription
      }
    }
  }
}
struct TrendsView: View {
  @EnvironmentObject private var device: DeviceController
  var body: some View {
    List {
      Section("Hourly detection activity") {
        Chart(device.trends) { t in
          BarMark(x: .value("Hour", t.date, unit: .hour), y: .value("Episodes", t.count))
            .foregroundStyle(.mint)
        }.frame(height: 240)
        Text(
          "\(device.episodes.count) saved episodes · \(device.episodes.filter(\.speeding).count) above 20 mph"
        ).font(.subheadline)
      }
      Section("Hourly peak speed") {
        Chart(device.trends) { t in
          PointMark(x: .value("Hour", t.date), y: .value("Peak MPH", Double(t.peak) / 1.609344))
            .foregroundStyle(.orange)
        }.frame(height: 200)
      }
      Section {
        Text(
          "Calendar charts omit unknown timestamps. Hourly bins use UTC boundaries and display in your local timezone. Episode totals are not vehicle counts."
        ).font(.caption).foregroundStyle(.secondary)
      }
    }.navigationTitle("Trends")
  }
}
struct DeviceView: View {
  @EnvironmentObject private var device: DeviceController
  var body: some View {
    List {
      Section("Connection") {
        LabeledContent("Bluetooth", value: device.connection)
        Button("Find nearby sign", systemImage: "antenna.radiowaves.left.and.right") {
          device.scan()
        }
        ForEach(device.devices) { nearby in
          Button {
            device.connect(nearby)
          } label: {
            HStack {
              Text(nearby.peripheral.name ?? "Street Speed")
              Spacer()
              Text(String(nearby.id.uuidString.prefix(8))).font(.caption).foregroundStyle(
                .secondary)
            }
          }
        }
        if device.status != nil { Button("Disconnect") { device.disconnect() } }
        Text(
          "First connection: use the sign’s USB-console ‘pair’ command, then enter the six-digit passkey shown there. Follow the repository’s isolated USB programming setup."
        ).font(.caption).foregroundStyle(.secondary)
      }
      Section("History download") {
        Button("Sync now", systemImage: "arrow.triangle.2.circlepath") {
          Task { await device.requestSync() }
        }.disabled(device.status == nil || device.syncing || device.store == nil)
        if device.syncing { ProgressView(value: device.syncProgress) }
        Text(device.syncMessage).font(.subheadline)
        if let date = device.lastSync {
          LabeledContent("Last completed sync") {
            Text(date, format: .dateTime.month().day().hour().minute())
          }
        }
      }
      if let status = device.status {
        Section("Sign status") {
          LabeledContent("Device", value: status.device)
          LabeledContent("Storage", value: status.storage)
          if status.capacity > 0 {
            ProgressView(value: Double(status.used), total: Double(status.capacity))
            Text("\(status.used/1024) / \(status.capacity/1024) KiB").font(.caption)
          }
          LabeledContent("Retained records", value: "\(status.oldest)…\(status.high)")
          LabeledContent("Acknowledged through", value: status.ack)
          LabeledContent("Dropped records", value: String(status.dropped))
          LabeledContent(
            "UART errors / backlogs", value: "\(status.uartErrors) / \(status.backlogs)")
          LabeledContent(
            "Clock source",
            value: status.clockQuality == 1 ? "RTC" : status.clockQuality == 2 ? "Phone" : "Unknown"
          )
          LabeledContent(
            "RTC",
            value: status.rtcHealthy ? "Initialized this session" : "Unavailable or not initialized"
          )
          Button("Set clock from this iPhone", systemImage: "clock.badge.checkmark") {
            device.setClock()
          }
        }
      }
      if !device.gaps.isEmpty {
        Section("Recorded data gaps") {
          ForEach(Array(device.gaps.enumerated()), id: \.offset) { _, gap in
            Text(gap).font(.caption)
          }
        }
      }
      Section {
        Text(
          "Experimental firmware. RTC, radio throughput, display timing and battery endurance need validation on the assembled sign."
        ).font(.caption).foregroundStyle(.secondary)
      }
    }.navigationTitle("Device")
  }
}
struct ShareFiles: UIViewControllerRepresentable {
  let urls: [URL]
  func makeUIViewController(context: Context) -> UIActivityViewController {
    UIActivityViewController(activityItems: urls, applicationActivities: nil)
  }
  func updateUIViewController(_ controller: UIActivityViewController, context: Context) {}
}
