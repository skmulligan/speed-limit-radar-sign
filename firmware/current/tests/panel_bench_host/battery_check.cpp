// Synthetic host simulation only; no bench validation.
#define BATTERY_PANEL_BENCH
#include "../../src/panel_bench.cpp"
#include <iostream>

int main(int argc, char** argv) {
#ifdef DAYLIGHT_PANEL_BENCH
  constexpr uint64_t expectedDelayUs = 15000000, expectedDurationUs = 20000000;
  constexpr uint64_t expectedPulseUs = 200;
#else
  constexpr uint64_t expectedDelayUs = 5000000, expectedDurationUs = 10000000;
  constexpr uint64_t expectedPulseUs = 50;
#endif
  const bool cancel = argc > 1 && strcmp(argv[1], "cancel") == 0;
  const bool activeStop = argc > 1 && strcmp(argv[1], "stop") == 0;
  if (argc > 1 && strcmp(argv[1], "rollover") == 0)
    fakeUs = uint64_t(0xfffff000u) * 1000;
  setup();
  const uint64_t base = fakeUs;
  if (cancel) { Serial.inject("stop\n"); loop(); }
  while (fakeUs - base < expectedDelayUs - 2000) loop();
  assert(!testing && riseTimes.empty());
  if (cancel) {
    while (fakeUs - base < expectedDelayUs + expectedDurationUs + 5000000) loop();
    assert(!testing && !autoPending && riseTimes.empty());
  } else {
    while (!testing) { assert(fakeUs - base < expectedDelayUs + 10000); loop(); }
    assert(digits && !mapping && forcedEnable == -1 && !autoPending);
    const uint64_t started = fakeUs;
    const size_t firstFrame = frames.size();
    while (fakeUs - started < 1000000) loop();
    Serial.inject("probe-high\noe-off-high\ndigits\n");
    for (int n = 0; n < 5; ++n) loop();
    assert(testing && digits && forcedEnable == -1 && offLevel == LOW);
    if (activeStop) { Serial.inject("stop\n"); loop(); }
    while (testing) { assert(fakeUs - started < expectedDurationUs + 5000); loop(); }
    if (!activeStop) assert(fakeUs - started >= expectedDurationUs - 2000);
    assert(!pins[9] && !digits);
    // Independent per-phase lit-pixel counts for the previously checked "20".
    const unsigned lit[] = {20, 40, 40, 20};
    bool sawDigits = false;
    for (size_t n = firstFrame; n < frames.size(); ++n) {
      const auto& f = frames[n];
      assert(f.bits == 128);
      assert(f.ones == 128 || f.ones == 128 - lit[f.phase]);
      sawDigits |= f.ones != 128;
    }
    assert(sawDigits && !pulseWidths.empty());
    for (auto width : pulseWidths) assert(width == expectedPulseUs);
    const size_t pulses = riseTimes.size();
    const uint64_t ended = fakeUs;
    Serial.inject("digits\n");
    while (fakeUs - ended < 20000000) loop();
    assert(!testing && !autoPending && !pins[9] && riseTimes.size() == pulses);
  }
  std::cout << "PASS synthetic standalone fixed-20 timing, pulses, blanking, and command limits\n";
}
