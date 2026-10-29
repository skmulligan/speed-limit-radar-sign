#include "../../src/panel_bench.cpp"
#include <iostream>
int main() {
  bool seen[16][32]{};
  unsigned markedPixels = 0;
  for (uint8_t phase = 0; phase < 4; ++phase) {
    for (unsigned bit = 0; bit < 128; ++bit) {
      const auto p = panel_mapping::fromSerialBit(phase, bit);
      assert(p.x < 32 && p.y < 16 && !seen[p.y][p.x]);
      seen[p.y][p.x] = true;
      markedPixels += panel_mapping::orientationMark(p);
    }
  }
  assert(markedPixels == 48);
  assert(panel_mapping::fromSerialBit(0, 63).x == 15);
  assert(panel_mapping::fromSerialBit(0, 63).y == 0);
  assert(panel_mapping::fromSerialBit(3, 0).y == 15);
  setup();
  for (int n = 0; n < 50; ++n) loop();
  assert(!testing && riseTimes.empty());
  assert(frames.size() >= 8);
  for (size_t n = 0; n < frames.size(); ++n) {
    assert(frames[n].phase == n % 4);
    assert(frames[n].bits == 128 && frames[n].ones == 128);
  }
  Serial.inject("test\n"); loop();
  assert(testing);
  const uint64_t testAt = fakeUs;
  const size_t firstTestFrame = frames.size();
  while (fakeUs < testAt + 500000) loop();
  // Repeating test must not extend the three-second run.
  Serial.inject("test\n"); loop();
  assert(Serial.output.find("ERROR test_already_running") != std::string::npos);
  while (testing) loop();
  assert(fakeUs - testAt < 3005000);
  assert(!pins[9] && !riseTimes.empty());
  for (auto width : pulseWidths) assert(width == 50);
  bool patternSeen = false;
  for (size_t n = firstTestFrame; n < frames.size(); ++n) {
    assert(frames[n].bits == 128);
    assert(frames[n].ones == 127 || frames[n].ones == 128);
    patternSeen |= frames[n].ones == 127;
  }
  assert(patternSeen);
  const size_t afterTest = frames.size();
  for (int n = 0; n < 30; ++n) loop();
  for (size_t n = afterTest; n < frames.size(); ++n) assert(frames[n].ones == 128);
  Serial.inject("test\n"); loop(); assert(testing);
  Serial.inject("stop\n"); loop(); assert(!testing && !pins[9]);
  Serial.inject("test123456789012345678901234567890123456789\n");
  for (int n = 0; n < 5; ++n) loop();
  assert(!testing);
  // Probe modes must hold ENABLE constant while the same sparse data scans,
  // then expire without requiring host intervention.
  for (int level : {LOW, HIGH}) {
    Serial.inject(level == LOW ? "probe-low\n" : "probe-high\n"); loop();
    assert(testing && forcedEnable == level);
    const uint64_t began = fakeUs;
    size_t frameStart = frames.size();
    while (frames.size() == frameStart) loop();
    while (testing) {
      assert(pins[9] == level);
      loop();
    }
    assert(fakeUs - began < 3005000 && forcedEnable == -1 && pins[9] == LOW);
    for (size_t n = frameStart; n + 1 < frames.size(); ++n) assert(frames[n].ones == 127);
  }
  // The mapping trial is longer but retains the short enable pulse and timeout.
  Serial.inject("map\n"); loop();
  assert(testing && mapping && forcedEnable == -1);
  const uint64_t mapAt = fakeUs;
  const size_t mapFrameStart = frames.size(), mapPulseStart = pulseWidths.size();
  while (fakeUs < mapAt + 5000000) loop();
  assert(testing);
  Serial.inject("map\n"); loop();
  while (testing) loop();
  assert(fakeUs - mapAt < 10005000 && !mapping && !pins[9]);
  for (size_t n = mapFrameStart; n + 1 < frames.size(); ++n) {
    assert(frames[n].ones == (frames[n].phase == 0 ? 92 : 124));
  }
  for (size_t n = mapPulseStart; n < pulseWidths.size(); ++n) assert(pulseWidths[n] == 50);
  // Fixed synthetic 20: verify glyph orientation independently of scan packing.
  const char* glyphRows[] = {".###...###.", "#...#.#...#", "....#.#...#",
                            "...#..#...#", "..#...#...#", ".#....#...#",
                            "#####..###."};
  unsigned digitPixels = 0, phasePixels[4]{};
  for (uint8_t y = 0; y < 16; ++y) {
    for (uint8_t x = 0; x < 32; ++x) {
      bool expected = false;
      if (y >= 1 && y <= 14 && x >= 5 && x <= 26)
        expected = glyphRows[(y - 1) / 2][(x - 5) / 2] == '#';
      assert(panel_mapping::testNumber20({x, y}) == expected);
      digitPixels += expected;
      phasePixels[y % 4] += expected;
    }
  }
  assert(digitPixels == 120);
  Serial.inject("digits\n"); loop();
  assert(testing && digits && !mapping && forcedEnable == -1);
  const uint64_t digitsAt = fakeUs;
  const size_t digitFrameStart = frames.size(), digitPulseStart = pulseWidths.size();
  while (fakeUs < digitsAt + 5000000) loop();
  Serial.inject("map\n"); loop(); // Cannot replace or extend an active trial.
  assert(digits && !mapping);
  while (testing) loop();
  assert(fakeUs - digitsAt < 10005000 && !digits && !pins[9]);
  for (size_t n = digitFrameStart; n + 1 < frames.size(); ++n)
    assert(frames[n].ones == 128 - phasePixels[frames[n].phase]);
  for (size_t n = digitPulseStart; n < pulseWidths.size(); ++n) assert(pulseWidths[n] == 50);
  Serial.inject("digits\n"); loop();
  Serial.inject("stop\n"); loop();
  assert(!testing && !digits && !pins[9]);
  // Inverted hypotheses are configurable; no physical blanking is inferred.
  Serial.inject("oe-off-high\nblank-bit-0\n"); loop();
  assert(offLevel == HIGH && blankBit == LOW);
  for (int n = 0; n < 5; ++n) loop();
  assert(frames.back().ones == 0 && pins[9] == HIGH);
  // Unsigned timeout arithmetic remains correct across millis() rollover.
  testing = true; testStartedMs = 0xfffffff0u;
  fakeUs = (uint64_t(0xfffffff0u) + 3001) * 1000;
  loop(); assert(!testing);
  std::cout << "PASS simulated GPIO: row cycling, frames, sparse pulses, bounded test/probes, stop, invalid commands, polarity configuration, timeout rollover\n";
}
