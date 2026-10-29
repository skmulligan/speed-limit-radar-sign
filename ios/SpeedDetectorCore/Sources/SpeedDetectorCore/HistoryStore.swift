import CSQLite
import Foundation

public struct DetectionEpisodeSummary: Identifiable, Codable, Sendable {
  public let device: String, boot: String
  public let event: Int64, startMono: Int64, lastMono: Int64, utc: Int64
  public let quality: Int, reason: Int, samples: Int64, sum: Int64, peak: Int, targets: Int
  public var id: String { "\(device)/\(boot)/\(event)" }
  public var date: Date? { utc > 0 ? Date(timeIntervalSince1970: Double(utc) / 1000) : nil }
  public var peakMPH: Double { Double(peak) / 1.609344 }
  public var meanMPH: Double { samples > 0 ? Double(sum) / Double(samples) / 1.609344 : 0 }
  public var duration: Double { Double(max(0, lastMono - startMono)) / 1000 }
  public var speeding: Bool { Double(peak) / 1.609344 > 20 }
  public var status: String {
    switch reason {
    case 0: return "Open"
    case 6: return "Interrupted by restart"
    case 7: return "Incomplete: storage gap"
    default: return "Ended"
    }
  }
  public var timeLabel: String {
    switch quality {
    case 1: return "RTC"
    case 2: return "Phone"
    case 3: return "Reconstructed"
    default: return "Unknown calendar time"
    }
  }
}
public struct SpeedBucket: Identifiable, Codable, Sendable {
  public let mono: Int64, duration: Int, minimum: Int, maximum: Int, sum: Int64, count: Int64,
    targets: Int
  public var id: Int64 { mono }
  public var meanMPH: Double { Double(sum) / Double(max(1, count)) / 1.609344 }
}
public struct HourlyTrend: Identifiable, Sendable {
  public let hour: Int64, count: Int, speeding: Int, peak: Int
  public var id: Int64 { hour }
  public var date: Date { Date(timeIntervalSince1970: Double(hour) * 3600) }
}
public struct ImportResult: Sendable { public let cursor: UInt64, complete: Bool }
public enum DatabaseError: Error, LocalizedError {
  case sqlite(String)
  case unsupportedSchema
  public var errorDescription: String? {
    switch self {
    case .sqlite(let s): return s
    case .unsupportedSchema: return "Database was created by a newer app version."
    }
  }
}
private enum SQLValue {
  case text(String)
  case integer(Int64)
  case blob(Data)
  case null
}
private let transient = unsafeBitCast(-1, to: sqlite3_destructor_type.self)

// Serial actor keeps filesystem work away from the live UI and serializes imports.
public actor HistoryStore {
  private var db: OpaquePointer?
  public init(path: String) throws {
    var handle: OpaquePointer?
    guard sqlite3_open(path, &handle) == SQLITE_OK else {
      if let handle { sqlite3_close(handle) }
      throw DatabaseError.sqlite("Cannot open history database")
    }
    db = handle
    do {
      try Self.exec(
        handle,
        "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA busy_timeout=2000;"
      )
      var statement: OpaquePointer?
      sqlite3_prepare_v2(handle, "PRAGMA user_version", -1, &statement, nil)
      let version = sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_int(statement, 0) : -1
      sqlite3_finalize(statement)
      guard version == 0 || version == 1 else { throw DatabaseError.unsupportedSchema }
      try Self.exec(
        handle,
        """
        CREATE TABLE IF NOT EXISTS devices(id TEXT PRIMARY KEY,cursor INTEGER NOT NULL DEFAULT 0);
        CREATE TABLE IF NOT EXISTS records(device TEXT NOT NULL,seq INTEGER NOT NULL,bytes BLOB NOT NULL,PRIMARY KEY(device,seq));
        CREATE TABLE IF NOT EXISTS episodes(device TEXT NOT NULL,boot TEXT NOT NULL,event INTEGER NOT NULL,startMono INTEGER NOT NULL,lastMono INTEGER NOT NULL,utc INTEGER NOT NULL,quality INTEGER NOT NULL,reason INTEGER NOT NULL DEFAULT 0,samples INTEGER NOT NULL DEFAULT 0,sum INTEGER NOT NULL DEFAULT 0,peak INTEGER NOT NULL DEFAULT 0,targets INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(device,boot,event));
        CREATE TABLE IF NOT EXISTS buckets(device TEXT NOT NULL,boot TEXT NOT NULL,event INTEGER NOT NULL,mono INTEGER NOT NULL,duration INTEGER NOT NULL,minimum INTEGER NOT NULL,maximum INTEGER NOT NULL,sum INTEGER NOT NULL,count INTEGER NOT NULL,targets INTEGER NOT NULL,PRIMARY KEY(device,boot,event,mono));
        CREATE TABLE IF NOT EXISTS gaps(device TEXT NOT NULL,seq INTEGER NOT NULL,description TEXT NOT NULL,PRIMARY KEY(device,seq));
        CREATE INDEX IF NOT EXISTS episode_dates ON episodes(utc);
        PRAGMA user_version=1;
        """)
    } catch {
      sqlite3_close(handle)
      db = nil
      throw error
    }
  }
  deinit { sqlite3_close(db) }
  private static func exec(_ db: OpaquePointer?, _ sql: String) throws {
    guard sqlite3_exec(db, sql, nil, nil, nil) == SQLITE_OK else {
      throw DatabaseError.sqlite(String(cString: sqlite3_errmsg(db)))
    }
  }
  @discardableResult private func query(_ sql: String, _ values: [SQLValue] = []) throws
    -> [[SQLValue]]
  {
    var s: OpaquePointer?
    guard sqlite3_prepare_v2(db, sql, -1, &s, nil) == SQLITE_OK else {
      throw DatabaseError.sqlite(String(cString: sqlite3_errmsg(db)))
    }
    defer { sqlite3_finalize(s) }
    for (i, v) in values.enumerated() {
      let index = Int32(i + 1)
      switch v {
      case .text(let t): sqlite3_bind_text(s, index, t, -1, transient)
      case .integer(let n): sqlite3_bind_int64(s, index, n)
      case .blob(let b):
        _ = b.withUnsafeBytes {
          sqlite3_bind_blob(s, index, $0.baseAddress, Int32(b.count), transient)
        }
      case .null: sqlite3_bind_null(s, index)
      }
    }
    var rows: [[SQLValue]] = []
    while true {
      let result = sqlite3_step(s)
      if result == SQLITE_DONE { return rows }
      guard result == SQLITE_ROW else {
        throw DatabaseError.sqlite(String(cString: sqlite3_errmsg(db)))
      }
      var row: [SQLValue] = []
      for c in 0..<sqlite3_column_count(s) {
        switch sqlite3_column_type(s, c) {
        case SQLITE_INTEGER: row.append(.integer(sqlite3_column_int64(s, c)))
        case SQLITE_TEXT: row.append(.text(String(cString: sqlite3_column_text(s, c))))
        case SQLITE_BLOB:
          row.append(
            .blob(Data(bytes: sqlite3_column_blob(s, c), count: Int(sqlite3_column_bytes(s, c)))))
        default: row.append(.null)
        }
      }
      rows.append(row)
    }
  }
  public func cursor(device: String) throws -> UInt64 {
    UInt64(
      try query("SELECT cursor FROM devices WHERE id=?", [.text(device)]).first?.first?.int ?? 0)
  }
  public func importBatch(_ batch: HistoryBatch, device: String) throws -> ImportResult {
    guard !device.isEmpty else { throw ProtocolError.malformed }
    try Self.exec(db, "BEGIN IMMEDIATE")
    do {
      try query("INSERT OR IGNORE INTO devices(id) VALUES(?)", [.text(device)])
      var cursor = try cursor(device: device)
      // Loss of older, previously acknowledged device logs after reinstall is explicit.
      if cursor + 1 < batch.oldest {
        try query(
          "INSERT OR REPLACE INTO gaps VALUES(?,?,?)",
          [
            .text(device), .integer(Int64(cursor + 1)),
            .text("Device retention gap: sequences \(cursor+1)…\(batch.oldest-1)"),
          ])
        cursor = batch.oldest - 1
      }
      for r in batch.records {
        if let row = try query(
          "SELECT bytes FROM records WHERE device=? AND seq=?",
          [.text(device), .integer(Int64(r.sequence))]
        ).first {
          guard row[0].data == r.bytes else { throw ProtocolError.conflict }
          continue
        }
        guard r.sequence == cursor + 1 else { throw ProtocolError.sequence }
        try query(
          "INSERT INTO records VALUES(?,?,?)",
          [.text(device), .integer(Int64(r.sequence)), .blob(r.bytes)])
        try apply(r, device: device)
        cursor = r.sequence
      }
      try query("UPDATE devices SET cursor=? WHERE id=?", [.integer(Int64(cursor)), .text(device)])
      try Self.exec(db, "COMMIT")
      return ImportResult(cursor: cursor, complete: cursor >= batch.high)
    } catch {
      try? Self.exec(db, "ROLLBACK")
      throw error
    }
  }
  private func apply(_ r: LogRecord, device: String) throws {
    let key: [SQLValue] = [.text(device), .text(r.boot), .integer(Int64(r.event))]
    if r.kind == .session {
      try query(
        "UPDATE episodes SET reason=6 WHERE device=? AND boot<>? AND reason=0",
        [.text(device), .text(r.boot)])
    }
    if r.kind == .clock || r.kind == .session, r.utc > 0 {
      // A later anchor can reconstruct unknown dates from this same continuous boot only.
      try query(
        "UPDATE episodes SET utc=?+(startMono-?),quality=3 WHERE device=? AND boot=? AND utc=0",
        [.integer(r.utc), .integer(Int64(r.mono)), .text(device), .text(r.boot)])
    }
    switch r.kind {
    case .start:
      try query(
        "INSERT OR IGNORE INTO episodes(device,boot,event,startMono,lastMono,utc,quality) VALUES(?,?,?,?,?,?,?)",
        key + [
          .integer(Int64(r.mono)), .integer(Int64(r.mono)), .integer(r.utc),
          .integer(Int64(r.quality)),
        ])
    case .bucket:
      // Retained segments can begin mid-event if this phone lost its old database.
      try query(
        "INSERT OR IGNORE INTO episodes(device,boot,event,startMono,lastMono,utc,quality,reason) VALUES(?,?,?,?,?,?,?,7)",
        key + [
          .integer(Int64(r.mono)), .integer(Int64(r.mono)), .integer(r.utc),
          .integer(Int64(r.quality)),
        ])
      let v = r.values
      try query(
        "INSERT INTO buckets VALUES(?,?,?,?,?,?,?,?,?,?)",
        key + [
          .integer(Int64(r.mono)), .integer(Int64(v[0])), .integer(Int64(v[1])),
          .integer(Int64(v[2])), .integer(Int64(v[3])), .integer(Int64(v[4])),
          .integer(Int64(v[5])),
        ])
      try query(
        "UPDATE episodes SET lastMono=MAX(lastMono,?),samples=samples+?,sum=sum+?,peak=MAX(peak,?),targets=MAX(targets,?) WHERE device=? AND boot=? AND event=?",
        [
          .integer(Int64(r.mono) + Int64(v[6]) + Int64(v[0])), .integer(Int64(v[4])),
          .integer(Int64(v[3])), .integer(Int64(v[2])), .integer(Int64(v[5])),
        ] + key)
    case .end:
      let end = UInt64(r.values[1]) | UInt64(r.values[2]) << 32
      guard end <= UInt64(Int64.max) else { throw ProtocolError.malformed }
      try query(
        "UPDATE episodes SET lastMono=MAX(lastMono,?),reason=CASE WHEN reason=7 THEN 7 ELSE ? END WHERE device=? AND boot=? AND event=?",
        [.integer(Int64(end)), .integer(Int64(r.values[0]))] + key)
    case .gap:
      try query(
        "INSERT INTO gaps VALUES(?,?,?)",
        [
          .text(device), .integer(Int64(r.sequence)),
          .text(
            r.values[0] == 2
              ? "Corrupt or interrupted journal record"
              : "Logging stopped; dropped records: \(r.values[1])"),
        ])
      try query("UPDATE episodes SET reason=7 WHERE device=? AND reason=0", [.text(device)])
    default: break
    }
  }
  public func episodes() throws -> [DetectionEpisodeSummary] {
    try query(
      "SELECT device,boot,event,startMono,lastMono,utc,quality,reason,samples,sum,peak,targets FROM episodes ORDER BY utc DESC,rowid DESC"
    ).map { r in
      DetectionEpisodeSummary(
        device: r[0].text, boot: r[1].text, event: r[2].int, startMono: r[3].int,
        lastMono: r[4].int, utc: r[5].int, quality: Int(r[6].int), reason: Int(r[7].int),
        samples: r[8].int, sum: r[9].int, peak: Int(r[10].int), targets: Int(r[11].int))
    }
  }
  public func buckets(_ episode: DetectionEpisodeSummary) throws -> [SpeedBucket] {
    try query(
      "SELECT mono,duration,minimum,maximum,sum,count,targets FROM buckets WHERE device=? AND boot=? AND event=? ORDER BY mono",
      [.text(episode.device), .text(episode.boot), .integer(episode.event)]
    ).map { r in
      SpeedBucket(
        mono: r[0].int, duration: Int(r[1].int), minimum: Int(r[2].int), maximum: Int(r[3].int),
        sum: r[4].int, count: r[5].int, targets: Int(r[6].int))
    }
  }
  public func trends() throws -> [HourlyTrend] {
    try query(
      "SELECT utc/3600000,COUNT(*),SUM(CASE WHEN peak>=33 THEN 1 ELSE 0 END),MAX(peak) FROM episodes WHERE utc>0 GROUP BY utc/3600000 ORDER BY utc/3600000"
    ).map {
      HourlyTrend(
        hour: $0[0].int, count: Int($0[1].int), speeding: Int($0[2].int), peak: Int($0[3].int))
    }
  }
  public func gaps() throws -> [String] {
    try query("SELECT device,seq,description FROM gaps ORDER BY rowid DESC").map {
      "\($0[0].text): \($0[2].text) (record \($0[1].int))"
    }
  }
  public func exportCSV() throws -> (episodes: String, buckets: String) {
    let rows = try episodes()
    let formatter = ISO8601DateFormatter()
    var summary =
      "device,boot,event,start_utc,time_quality,duration_seconds,peak_kmh,mean_kmh,peak_mph,mean_mph,samples,max_targets,status\n"
    var detail =
      "device,boot,event,elapsed_seconds,observed_span_ms,min_kmh,max_kmh,mean_kmh,samples,max_targets\n"
    func cell(_ s: String) -> String {
      "\"" + s.replacingOccurrences(of: "\"", with: "\"\"") + "\""
    }
    for e in rows {
      let mean = e.samples > 0 ? Double(e.sum) / Double(e.samples) : 0
      summary +=
        [
          e.device, e.boot, String(e.event), e.date.map { formatter.string(from: $0) } ?? "",
          e.timeLabel, String(e.duration), String(e.peak), String(mean), String(e.peakMPH),
          String(e.meanMPH), String(e.samples), String(e.targets), e.status,
        ].map(cell).joined(separator: ",") + "\n"
      for b in try buckets(e) {
        detail +=
          [
            e.device, e.boot, String(e.event), String(Double(b.mono - e.startMono) / 1000),
            String(b.duration), String(b.minimum), String(b.maximum),
            String(Double(b.sum) / Double(b.count)), String(b.count), String(b.targets),
          ].map(cell).joined(separator: ",") + "\n"
      }
    }
    return (summary, detail)
  }
}
extension SQLValue {
  fileprivate var int: Int64 {
    if case .integer(let n) = self { return n }
    return 0
  }
  fileprivate var text: String {
    if case .text(let t) = self { return t }
    return ""
  }
  fileprivate var data: Data? {
    if case .blob(let b) = self { return b }
    return nil
  }
}
