import XCTest

@testable import SpeedDetectorCore

final class CoreTests: XCTestCase {
  func fixture() throws -> Data {
    try Data(
      contentsOf: XCTUnwrap(
        Bundle.module.url(
          forResource: "synthetic-episode", withExtension: "bin", subdirectory: "Fixtures")))
  }
  func batch(_ bytes: Data, high: UInt64 = 4, oldest: UInt64 = 1) throws -> HistoryBatch {
    var header = Data()
    for n in [high, oldest, 123] { for i in 0..<8 { header.append(UInt8((n >> (i * 8)) & 255)) } }
    return try HistoryBatch(header + bytes)
  }
  func testGoldenCPPFormatAndFragmentation() throws {
    let bytes = try fixture()
    let b = try batch(bytes)
    XCTAssertEqual(b.records.count, 4)
    XCTAssertEqual(b.transferID, 123)
    XCTAssertEqual(b.records[1].values[3], 120)
    XCTAssertEqual(b.records[2].mono, 1100)
    var decoder = FragmentDecoder()
    var complete: Data?
    for packet in fragments(bytes, id: 5) { complete = try decoder.accept(packet) }
    XCTAssertEqual(complete, bytes)
    for limit in [20, 182, 244] {
      decoder.reset()
      for packet in fragments(bytes, id: 7, valueLimit: limit) {
        XCTAssertLessThanOrEqual(packet.count, limit)
        complete = try decoder.accept(packet)
      }
      XCTAssertEqual(complete, bytes)
    }
    var corrupted = bytes
    corrupted[60] ^= 1
    XCTAssertThrowsError(try LogRecord(corrupted.prefix(80)))
    var packets = fragments(bytes, id: 6)
    packets.remove(at: 1)
    decoder.reset()
    _ = try decoder.accept(packets[0])
    XCTAssertThrowsError(try decoder.accept(packets[1]))
  }
  func testImportIsIdempotentAndSurvivesReopen() async throws {
    let path = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString).path
    defer {
      try? FileManager.default.removeItem(atPath: path)
      try? FileManager.default.removeItem(atPath: path + "-wal")
      try? FileManager.default.removeItem(atPath: path + "-shm")
    }
    let store = try HistoryStore(path: path)
    let b = try batch(fixture())
    _ = try await store.importBatch(b, device: "test")
    _ = try await store.importBatch(b, device: "test")
    let episodes = try await store.episodes()
    XCTAssertEqual(episodes.count, 1)
    XCTAssertEqual(episodes[0].samples, 4)
    XCTAssertEqual(episodes[0].sum, 180)
    XCTAssertEqual(episodes[0].peak, 60)
    let reopened = try HistoryStore(path: path)
    let cursor = try await reopened.cursor(device: "test")
    XCTAssertEqual(cursor, 4)
    let csv = try await store.exportCSV()
    XCTAssertTrue(csv.episodes.contains("peak_mph"))
    XCTAssertEqual(csv.buckets.split(separator: "\n").count, 3)
  }
  func testMissingSequenceRollsBackWholeBatch() async throws {
    let store = try HistoryStore(path: ":memory:")
    let bytes = try fixture()
    // Sequence 2 cannot start a transfer when the device still retains sequence 1.
    do {
      _ = try await store.importBatch(batch(bytes.subdata(in: 80..<160)), device: "test")
      XCTFail("accepted gap")
    } catch {}
    let cursor = try await store.cursor(device: "test")
    let episodes = try await store.episodes()
    XCTAssertEqual(cursor, 0)
    XCTAssertTrue(episodes.isEmpty)
  }
  func testRetentionGapIsExplicit() async throws {
    let store = try HistoryStore(path: ":memory:")
    let bytes = try fixture()
    _ = try await store.importBatch(batch(bytes.subdata(in: 80..<320), oldest: 2), device: "test")
    let gaps = try await store.gaps()
    let episodes = try await store.episodes()
    XCTAssertEqual(gaps.count, 1)
    XCTAssertEqual(episodes.first?.reason, 7)
  }
  func testLoggingQueueGapClosesOldEpisodeBeforeRecovery() async throws {
    // Synthetic v1 sequence: accepted Start/Bucket, queue-loss Gap, new Start.
    // No invented End or repeated event ID is needed after a worker pause.
    let store = try HistoryStore(path: ":memory:")
    let original = try fixture()
    func rewrite(_ source: Data, kind: UInt8, sequence: UInt64, event: UInt64,
                 mono: UInt64, values: [UInt64]? = nil) -> Data {
      var bytes = source
      func put(_ value: UInt64, _ offset: Int, _ count: Int) {
        for i in 0..<count { bytes[offset + i] = UInt8((value >> (8 * i)) & 255) }
      }
      bytes[5] = kind
      put(sequence, 8, 8); put(mono, 24, 8); put(event, 32, 8)
      if let values { for i in 0..<7 { put(values[i], 48 + i * 4, 4) } }
      put(UInt64(crc32(bytes.prefix(76))), 76, 4)
      return bytes
    }
    let gap = rewrite(original.subdata(in: 0..<80), kind: 6, sequence: 3,
                      event: 0, mono: 1500, values: [1, 2, 1200, 0, 0, 0, 0])
    let resumed = rewrite(original.subdata(in: 0..<80), kind: 2, sequence: 4,
                          event: 2, mono: 1600)
    let records = original.subdata(in: 0..<160) + gap + resumed
    _ = try await store.importBatch(batch(records), device: "test")
    _ = try await store.importBatch(batch(records), device: "test")
    let episodes = try await store.episodes()
    XCTAssertEqual(episodes.count, 2)
    let old = try XCTUnwrap(episodes.first { $0.startMono == 100 })
    let new = try XCTUnwrap(episodes.first { $0.startMono == 1600 })
    XCTAssertEqual(old.reason, 7)
    XCTAssertEqual(old.samples, 3)
    XCTAssertEqual(new.reason, 0)
    XCTAssertEqual(new.samples, 0)
    let gaps = try await store.gaps()
    XCTAssertEqual(gaps.count, 1)
  }
  func testConflictingDuplicateRollsBack() async throws {
    let store = try HistoryStore(path: ":memory:")
    let bytes = try fixture()
    _ = try await store.importBatch(batch(bytes), device: "test")
    var changed = bytes.subdata(in: 0..<80)
    changed[48] = 21
    let crc = crc32(changed.prefix(76))
    for i in 0..<4 { changed[76 + i] = UInt8((crc >> (8 * i)) & 255) }
    do {
      _ = try await store.importBatch(batch(changed), device: "test")
      XCTFail("accepted conflicting record")
    } catch {}
    let cursor = try await store.cursor(device: "test")
    XCTAssertEqual(cursor, 4)
  }
  func testFreshnessUsesConservativeClockAndRejectsReboot() throws {
    func snapshot(
      _ boot: String = "a", _ seq: String = "1", _ sample: Double = 1000, _ state: String = "valid"
    ) throws -> LiveRadarSnapshot {
      let json =
        "{\"v\":1,\"boot\":\"\(boot)\",\"sequence\":\"\(seq)\",\"sampleMono\":\(sample),\"sentMono\":1100,\"state\":\"\(state)\",\"selected\":0,\"warning\":true,\"alarm\":1,\"targets\":[[0,10,1,40,80]]}"
      return try JSONDecoder().decode(LiveRadarSnapshot.self, from: Data(json.utf8))
    }
    var clock = LiveClock()
    let first = try snapshot()
    XCTAssertFalse(clock.accept(first))
    clock.anchor(boot: "a", deviceMono: 1000, sentAt: 10, receivedAt: 10.1)
    XCTAssertTrue(clock.accept(first))
    XCTAssertTrue(clock.fresh(first, now: 10.749))
    XCTAssertFalse(clock.fresh(first, now: 10.750))
    XCTAssertFalse(clock.accept(try snapshot("b")))
    XCTAssertTrue(clock.accept(try snapshot("a", "2")))
    XCTAssertFalse(clock.accept(first))
    XCTAssertFalse(clock.fresh(try snapshot("a", "3", 1000, "fault"), now: 10.2))
    XCTAssertFalse(clock.fresh(try snapshot("a", "3", 1100), now: 10.01))
    XCTAssertFalse(clock.fresh(first, now: 20.1))
  }

  func testClockAnchorReconstructsOnlySameBootAndRestartMarksIncomplete() async throws {
    let store = try HistoryStore(path: ":memory:")
    func record(kind: UInt8, seq: UInt64, boot: UInt64, mono: UInt64, utc: UInt64) throws -> Data {
      var bytes = try fixture().subdata(in: 0..<80)
      func put(_ value: UInt64, at offset: Int, count: Int) {
        for i in 0..<count { bytes[offset + i] = UInt8((value >> (8 * i)) & 255) }
      }
      bytes[5] = kind
      put(utc == 0 ? 0 : 2, at: 6, count: 2)
      put(seq, at: 8, count: 8)
      put(boot, at: 16, count: 8)
      put(mono, at: 24, count: 8)
      put(utc, at: 40, count: 8)
      put(UInt64(crc32(bytes.prefix(76))), at: 76, count: 4)
      return bytes
    }
    let start = try record(kind: 2, seq: 1, boot: 42, mono: 100, utc: 0)
    let anchor = try record(kind: 5, seq: 2, boot: 42, mono: 1000, utc: 1_790_000_000_000)
    _ = try await store.importBatch(batch(start + anchor, high: 2), device: "test")
    let reconstructed = try await store.episodes()
    XCTAssertEqual(reconstructed[0].utc, 1_790_000_000_000 - 900)
    XCTAssertEqual(reconstructed[0].quality, 3)

    let unknown = try record(kind: 2, seq: 3, boot: 43, mono: 200, utc: 0)
    let nextSession = try record(kind: 1, seq: 4, boot: 44, mono: 0, utc: 1_790_000_001_000)
    _ = try await store.importBatch(batch(unknown + nextSession), device: "test")
    let episodes = try await store.episodes()
    let old = try XCTUnwrap(episodes.first { $0.boot == "000000000000002b" })
    XCTAssertEqual(old.utc, 0)
    XCTAssertEqual(old.reason, 6)
    let trends = try await store.trends()
    XCTAssertEqual(trends.reduce(0) { $0 + $1.count }, 1)
  }
}
