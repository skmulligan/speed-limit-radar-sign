import Foundation

public enum ProtocolError: Error { case malformed, checksum, version, sequence, conflict }
public enum RecordKind: UInt8 {
  case session = 1
  case start, bucket, end, clock, gap
}
public struct LogRecord {
  public static let size = 80
  public let bytes: Data
  public let kind: RecordKind
  public let quality: UInt16
  public let sequence: UInt64
  public let boot: String
  public let mono: UInt64
  public let event: UInt64
  public let utc: Int64
  public let values: [UInt32]
  public init(_ data: Data) throws {
    let b = Array(data)
    guard b.count == Self.size, little(b, 0, 4) == 0x3147_4453 else {
      throw ProtocolError.malformed
    }
    guard b[4] == 1 else { throw ProtocolError.version }
    guard let kind = RecordKind(rawValue: b[5]) else { throw ProtocolError.malformed }
    guard little(b, 76, 4) == UInt64(crc32(Data(b.prefix(76)))) else {
      throw ProtocolError.checksum
    }
    self.bytes = data
    self.kind = kind
    quality = UInt16(little(b, 6, 2))
    sequence = little(b, 8, 8)
    boot = String(format: "%016llx", little(b, 16, 8))
    mono = little(b, 24, 8)
    event = little(b, 32, 8)
    utc = Int64(bitPattern: little(b, 40, 8))
    values = (0..<7).map { UInt32(little(b, 48 + $0 * 4, 4)) }
    guard sequence > 0, sequence <= UInt64(Int64.max), mono <= UInt64(Int64.max),
      event <= UInt64(Int64.max), quality <= 2
    else { throw ProtocolError.malformed }
    if kind == .bucket {
      guard values[4] > 0, values[1] <= values[2], values[2] <= 120, values[5] <= 16,
        UInt64(values[3]) <= UInt64(values[4]) * 120, values[0] < 1000, values[6] < 1000,
        values[0] + values[6] < 1000
      else { throw ProtocolError.malformed }
    }
  }
}
public func little(_ b: [UInt8], _ offset: Int, _ count: Int) -> UInt64 {
  (0..<count).reduce(0) { $0 | UInt64(b[offset + $1]) << (8 * $1) }
}
public func crc32(_ data: Data) -> UInt32 {
  var crc = UInt32.max
  for byte in data {
    crc ^= UInt32(byte)
    for _ in 0..<8 { crc = (crc >> 1) ^ ((crc & 1) == 1 ? 0xedb8_8320 : 0) }
  }
  return ~crc
}
public struct FragmentDecoder {
  private var message: UInt16 = 0
  private var total = 0
  private var bytes = Data()
  public init() {}
  public mutating func reset() {
    bytes.removeAll()
    total = 0
  }
  public mutating func accept(_ packet: Data) throws -> Data? {
    let p = Array(packet)
    guard (7...244).contains(p.count) else {
      reset()
      throw ProtocolError.malformed
    }
    let id = UInt16(little(p, 0, 2))
    let count = Int(little(p, 2, 2))
    let offset = Int(little(p, 4, 2))
    if offset == 0 {
      reset()
      message = id
      total = count
    }
    guard id == message, count == total, (4...2048).contains(total), offset == bytes.count,
      offset + p.count - 6 <= total
    else {
      reset()
      throw ProtocolError.malformed
    }
    bytes.append(contentsOf: p.dropFirst(6))
    guard bytes.count == total else { return nil }
    let payload = Data(bytes.dropLast(4))
    let expected = little(Array(bytes), bytes.count - 4, 4)
    reset()
    guard UInt64(crc32(payload)) == expected else { throw ProtocolError.checksum }
    return payload
  }
}
public func fragments(_ payload: Data, id: UInt16, valueLimit: Int = 20) -> [Data] {
  guard payload.count <= 2044, (20...244).contains(valueLimit) else { return [] }
  let fragmentSize = valueLimit - 6
  var bytes = Array(payload)
  var crc = crc32(payload)
  for _ in 0..<4 {
    bytes.append(UInt8(crc & 255))
    crc >>= 8
  }
  return stride(from: 0, to: bytes.count, by: fragmentSize).map { offset in
    var packet: [UInt8] = []
    for value in [Int(id), bytes.count, offset] {
      packet += [UInt8(value & 255), UInt8(value >> 8)]
    }
    packet += bytes[offset..<min(offset + fragmentSize, bytes.count)]
    return Data(packet)
  }
}
public struct HistoryBatch {
  public let high: UInt64, oldest: UInt64, transferID: UInt64
  public let records: [LogRecord]
  public init(_ payload: Data) throws {
    let b = Array(payload)
    guard b.count >= 24, (b.count - 24) % 80 == 0, b.count <= 1304 else {
      throw ProtocolError.malformed
    }
    high = little(b, 0, 8)
    oldest = little(b, 8, 8)
    transferID = little(b, 16, 8)
    guard high < UInt64(Int64.max), oldest > 0, oldest <= high + 1 else {
      throw ProtocolError.malformed
    }
    records = try stride(from: 24, to: b.count, by: 80).map { try LogRecord(Data(b[$0..<$0 + 80])) }
    var prior: UInt64?
    for record in records {
      guard record.sequence >= oldest, record.sequence <= high,
        prior == nil || record.sequence == prior! + 1
      else { throw ProtocolError.sequence }
      prior = record.sequence
    }
  }
}
public struct LiveRadarSnapshot: Codable, Sendable {
  public let v: Int, boot: String, sequence: String, sampleMono: Double, sentMono: Double,
    state: String, selected: Int, warning: Bool, alarm: Int, targets: [[Int]]
  public func validShape() -> Bool {
    v == 1 && targets.count <= 16
      && targets.allSatisfy {
        $0.count == 5 && (-128...127).contains($0[0]) && (0...100).contains($0[1])
          && (0...1).contains($0[2]) && (0...120).contains($0[3]) && (0...255).contains($0[4])
      } && (selected == -1 || targets.indices.contains(selected))
      && (state != "valid" || targets.indices.contains(selected)) && sampleMono >= 0
      && sentMono >= sampleMono
  }
}
// Device time at ping processing lies between phone send and receive.
// Using send as its local origin makes age conservative by up to round trip time.
public struct LiveClock {
  public var boot: String = ""
  private var deviceAnchor = 0.0, localAnchor = 0.0
  private var lastSequence: UInt64 = 0
  private var lastSentMono = -1.0
  private var hasAnchor = false
  public init() {}
  public mutating func anchor(boot: String, deviceMono: Double, sentAt: Double, receivedAt: Double)
  {
    guard receivedAt >= sentAt, receivedAt - sentAt < 2 else { return }
    if self.boot != boot {
      lastSequence = 0
      lastSentMono = -1
    }
    self.boot = boot
    deviceAnchor = deviceMono
    localAnchor = sentAt
    hasAnchor = true
  }
  public mutating func accept(_ snapshot: LiveRadarSnapshot) -> Bool {
    guard hasAnchor, snapshot.boot == boot, snapshot.validShape(),
      let sequence = UInt64(snapshot.sequence), sequence >= lastSequence,
      snapshot.sentMono >= lastSentMono
    else { return false }
    lastSequence = sequence
    lastSentMono = snapshot.sentMono
    return true
  }
  public func age(_ snapshot: LiveRadarSnapshot, now: Double) -> Double? {
    guard hasAnchor, snapshot.boot == boot, now >= localAnchor, now - localAnchor < 10 else {
      return nil
    }
    let age = (now - localAnchor) * 1000 - (snapshot.sampleMono - deviceAnchor)
    return age >= 0 ? age : nil
  }
  public func fresh(_ snapshot: LiveRadarSnapshot, now: Double) -> Bool {
    ["valid", "no_eligible"].contains(snapshot.state) && snapshot.validShape()
      && (age(snapshot, now: now).map { $0 < 750 } ?? false)
  }
}
