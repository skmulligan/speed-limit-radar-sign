import CoreBluetooth
import SpeedDetectorCore
import SwiftUI

struct DeviceStatus: Decodable {
  let v: Int, response: String, token: String, device: String, boot: String, mono: Double
  let oldest: String, high: String, ack: String, transferHigh: String
  let used: Int, capacity: Int, storage: String, clockQuality: Int, rtcHealthy: Bool
  let dropped: Int, uartErrors: Int, invalidBytes: Int, backlogs: Int, commandDrops: Int
}
struct NearbyDevice: Identifiable {
  let peripheral: CBPeripheral
  let rssi: Int
  var id: UUID { peripheral.identifier }
}

// Core Bluetooth is explicitly created on the main queue; callbacks stay there.
@MainActor
final class DeviceController: NSObject, ObservableObject, @preconcurrency CBCentralManagerDelegate,
  @preconcurrency CBPeripheralDelegate
{
  static let service = CBUUID(string: "73640001-7065-6564-6465-746563746f72")
  static let statusID = CBUUID(string: "73640002-7065-6564-6465-746563746f72")
  static let controlID = CBUUID(string: "73640003-7065-6564-6465-746563746f72")
  static let liveID = CBUUID(string: "73640004-7065-6564-6465-746563746f72")
  static let historyID = CBUUID(string: "73640005-7065-6564-6465-746563746f72")
  @Published var connection = "Disconnected"
  @Published var devices: [NearbyDevice] = []
  @Published var status: DeviceStatus?
  @Published var snapshot: LiveRadarSnapshot?
  @Published var liveLabel = "Disconnected"
  @Published var fresh = false
  @Published var ageMS: Double?
  @Published var episodes: [DetectionEpisodeSummary] = []
  @Published var trends: [HourlyTrend] = []
  @Published var gaps: [String] = []
  @Published var syncMessage = "History is available offline"
  @Published var syncProgress = 0.0
  @Published var syncing = false
  @Published var errorMessage: String?
  @Published var lastSync: Date?
  let store: HistoryStore?
  private var central: CBCentralManager!
  private var peripheral: CBPeripheral?
  private var characteristics: [CBUUID: CBCharacteristic] = [:]
  private var decoders: [CBUUID: FragmentDecoder] = [:]
  private var clock = LiveClock()
  private var timer: Timer?
  private var lastPing = 0.0, lastHistory = 0.0
  private var pingID: UInt64 = 0
  private var pings: [String: Double] = [:]
  private var frameID: UInt16 = 0
  private var writeQueue: [(Data, String?)] = []
  private var writing = false, foreground = true
  private var ready = false, generation: UInt64 = 0
  private var syncStart: UInt64 = 0
  private var syncID: UInt64 = 0
  private var lastHistoryRefresh = 0.0
  override init() {
    var db: HistoryStore?
    var failure: String?
    let testing = ProcessInfo.processInfo.arguments.contains("--ui-testing")
    do {
      let directory = try FileManager.default.url(
        for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true
      ).appendingPathComponent("StreetSpeed", isDirectory: true)
      try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
      db = try HistoryStore(
        path: testing ? ":memory:" : directory.appendingPathComponent("history.sqlite").path)
    } catch { failure = error.localizedDescription }
    store = db
    super.init()
    errorMessage = failure
    lastSync = UserDefaults.standard.object(forKey: "lastSync") as? Date
    if !testing { central = CBCentralManager(delegate: self, queue: nil) }
    timer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
      Task { @MainActor in self?.tick() }
    }
    Task { await reloadHistory() }
  }
  func scan() {
    guard let central, central.state == .poweredOn else {
      errorMessage = "Enable Bluetooth and allow Street Speed to use it in Settings."
      return
    }
    devices = []
    connection = "Scanning nearby devices"
    central.scanForPeripherals(
      withServices: [Self.service], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
  }
  func connect(_ device: NearbyDevice) {
    if peripheral?.identifier == device.id, device.peripheral.state != .disconnected { return }
    central.stopScan()
    if let peripheral { central.cancelPeripheralConnection(peripheral) }
    resetConnection()
    peripheral = device.peripheral
    device.peripheral.delegate = self
    connection = "Connecting…"
    central.connect(device.peripheral)
  }
  func disconnect() {
    central.stopScan()
    if let peripheral { central.cancelPeripheralConnection(peripheral) }
    resetConnection()
  }
  private func resetConnection() {
    generation &+= 1
    ready = false
    characteristics = [:]
    decoders = [:]
    writeQueue = []
    writing = false
    pings = [:]
    clock = LiveClock()
    snapshot = nil
    fresh = false
    ageMS = nil
    status = nil
    syncing = false
    liveLabel = "Disconnected"
    connection = "Disconnected"
  }
  func setForeground(_ active: Bool) {
    foreground = active
    if !active {
      fresh = false
      snapshot = nil
      clock = LiveClock()
      liveLabel = "App inactive"
    } else if ready {
      snapshot = nil
      clock = LiveClock()
      send("STATUS")
      ping()
      if syncing { Task { await requestSync(resume: true) } }
    }
  }
  func setClock() { send("TIME \(Int64(Date().timeIntervalSince1970*1000))") }
  private func ping() {
    guard ready else { return }
    pingID &+= 1
    lastPing = ProcessInfo.processInfo.systemUptime
    send("PING \(pingID)", pingToken: String(pingID))
  }
  private func send(_ command: String, pingToken: String? = nil) {
    guard ready, writeQueue.count < 256 else { return }
    frameID &+= 1
    for (i, packet) in fragments(Data(command.utf8), id: frameID).enumerated() {
      writeQueue.append((packet, i == 0 ? pingToken : nil))
    }
    pump()
  }
  private func pump() {
    guard !writing, let peripheral, let control = characteristics[Self.controlID],
      !writeQueue.isEmpty
    else { return }
    writing = true
    if let token = writeQueue[0].1 { pings[token] = ProcessInfo.processInfo.systemUptime }
    peripheral.writeValue(writeQueue[0].0, for: control, type: .withResponse)
  }
  private func tick() {
    let now = ProcessInfo.processInfo.systemUptime
    if foreground, ready, now - lastPing > 4 { ping() }
    if foreground, ready, let snapshot {
      fresh = clock.fresh(snapshot, now: now)
      ageMS = clock.age(snapshot, now: now)
      liveLabel =
        fresh
        ? (snapshot.state == "no_eligible" ? "No eligible target" : "Live")
        : ["valid", "no_eligible"].contains(snapshot.state)
          ? "Reading expired" : label(snapshot.state)
    } else {
      fresh = false
    }
    if syncing, foreground, now - lastHistory > 8 {
      syncing = false
      syncMessage = "Download interrupted — tap Sync now to resume"
    }
    pings = pings.filter { now - $0.value < 10 }
  }
  private func label(_ state: String) -> String {
    switch state {
    case "no_target": return "No target"
    case "fault": return "Input fault"
    case "expired": return "Reading expired"
    case "starting": return "Starting"
    case "stopped": return "Stopped"
    default: return "Waiting for fresh data"
    }
  }
  func requestSync(resume: Bool = false) async {
    guard ready, let status, let store else { return }
    let expectedGeneration = generation
    do {
      syncStart = try await store.cursor(device: status.device)
      guard expectedGeneration == generation, ready else { return }
      syncID = UInt64.random(in: 1...UInt64.max)
      syncing = true
      syncMessage = resume ? "Resuming download…" : "Downloading…"
      syncProgress = 0
      lastHistory = ProcessInfo.processInfo.systemUptime
      send("SYNC \(syncID) \(syncStart)")
    } catch { errorMessage = error.localizedDescription }
  }
  func reloadHistory() async {
    guard let store else { return }
    do {
      episodes = try await store.episodes()
      trends = try await store.trends()
      gaps = try await store.gaps()
    } catch { errorMessage = error.localizedDescription }
  }
  func centralManagerDidUpdateState(_ central: CBCentralManager) {
    if central.state != .poweredOn {
      resetConnection()
      connection =
        central.state == .unauthorized ? "Bluetooth permission required" : "Bluetooth unavailable"
    }
  }
  func centralManager(
    _ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
    advertisementData: [String: Any], rssi rssiValue: NSNumber
  ) {
    if !devices.contains(where: { $0.id == peripheral.identifier }) {
      devices.append(NearbyDevice(peripheral: peripheral, rssi: rssiValue.intValue))
    }
  }
  func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
    guard self.peripheral?.identifier == peripheral.identifier else { return }
    connection = "Pairing / discovering services…"
    peripheral.discoverServices([Self.service])
  }
  func centralManager(
    _ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?
  ) {
    guard self.peripheral?.identifier == peripheral.identifier else { return }
    resetConnection()
    errorMessage = error?.localizedDescription ?? "Connection failed"
  }
  func centralManager(
    _ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?
  ) {
    guard self.peripheral?.identifier == peripheral.identifier else { return }
    resetConnection()
    syncMessage = "Disconnected — downloaded history is saved"
  }
  func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
    if let error {
      errorMessage = error.localizedDescription
      return
    }
    for service in peripheral.services ?? [] {
      peripheral.discoverCharacteristics(nil, for: service)
    }
  }
  func peripheral(
    _ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?
  ) {
    if let error {
      errorMessage = error.localizedDescription
      return
    }
    for c in service.characteristics ?? [] {
      characteristics[c.uuid] = c
      if [Self.statusID, Self.liveID, Self.historyID].contains(c.uuid) {
        peripheral.setNotifyValue(true, for: c)
      }
    }
  }
  func peripheral(
    _ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic,
    error: Error?
  ) {
    if let error {
      errorMessage = "Pairing/subscription failed: \(error.localizedDescription)"
      return
    }
    guard !ready, characteristics[Self.controlID] != nil,
      [Self.statusID, Self.liveID, Self.historyID].allSatisfy({
        characteristics[$0]?.isNotifying == true
      })
    else { return }
    ready = true
    connection = "Connected"
    liveLabel = "Waiting for fresh data"
    send("STATUS")
    ping()
  }
  func peripheral(
    _ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?
  ) {
    guard peripheral.identifier == self.peripheral?.identifier else { return }
    writing = false
    if let error {
      writeQueue = []
      errorMessage = error.localizedDescription
      return
    }
    if !writeQueue.isEmpty { writeQueue.removeFirst() }
    pump()
  }
  func peripheral(
    _ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?
  ) {
    guard peripheral.identifier == self.peripheral?.identifier, foreground, error == nil,
      let data = characteristic.value
    else { return }
    do {
      var decoder = decoders[characteristic.uuid] ?? FragmentDecoder()
      let payload: Data?
      do { payload = try decoder.accept(data) } catch {
        decoders[characteristic.uuid] = decoder
        throw error
      }
      decoders[characteristic.uuid] = decoder
      guard let payload else { return }
      if characteristic.uuid == Self.statusID {
        let value = try JSONDecoder().decode(DeviceStatus.self, from: payload)
        guard value.v == 1 else { throw ProtocolError.version }
        if let status, status.boot != value.boot || status.device != value.device {
          clock = LiveClock()
          snapshot = nil
          fresh = false
          syncing = false
          generation &+= 1
        }
        status = value
        if value.response == "pong", let sent = pings.removeValue(forKey: value.token) {
          clock.anchor(
            boot: value.boot, deviceMono: value.mono, sentAt: sent,
            receivedAt: ProcessInfo.processInfo.systemUptime)
        }
        if value.response.hasPrefix("bad_") {
          errorMessage = "Device rejected request: \(value.response)"
          syncing = false
        }
      } else if characteristic.uuid == Self.liveID {
        let value = try JSONDecoder().decode(LiveRadarSnapshot.self, from: payload)
        if clock.accept(value) {
          snapshot = value
          tick()
        }
      } else if characteristic.uuid == Self.historyID, let status {
        let batch = try HistoryBatch(payload)
        guard syncing, batch.transferID == syncID else { return }
        let expectedGeneration = generation
        lastHistory = ProcessInfo.processInfo.systemUptime
        Task {
          guard let store else { return }
          do {
            let result = try await store.importBatch(batch, device: status.device)
            guard expectedGeneration == generation, batch.transferID == syncID else { return }
            send("ACK \(result.cursor)")
            syncProgress =
              batch.high > syncStart
              ? Double(result.cursor - syncStart) / Double(batch.high - syncStart) : 1
            if result.complete || ProcessInfo.processInfo.systemUptime - lastHistoryRefresh > 2 {
              lastHistoryRefresh = ProcessInfo.processInfo.systemUptime
              await reloadHistory()
            }
            guard expectedGeneration == generation, batch.transferID == syncID else { return }
            if result.complete {
              syncing = false
              syncMessage = "History is up to date"
              lastSync = Date()
              UserDefaults.standard.set(lastSync, forKey: "lastSync")
            } else {
              lastHistory = ProcessInfo.processInfo.systemUptime
              send("NEXT \(syncID) \(result.cursor)")
            }
          } catch {
            syncing = false
            syncMessage = "Download stopped; existing history is safe"
            errorMessage = error.localizedDescription
          }
        }
      }
    } catch {
      if characteristic.uuid == Self.liveID {
        snapshot = nil
        fresh = false
        liveLabel = "Invalid live data"
      } else {
        errorMessage = "Invalid Bluetooth data; retry connection or sync. \(error)"
      }
    }
  }
}
