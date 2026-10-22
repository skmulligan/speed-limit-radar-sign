// One-panel HUB12 bring-up. Mapping was exercised on the recorded panel only.
// Defaults follow Freetronics DMD's implemented polarity, not its nOE comment.
// No radar or persistent settings. BATTERY_PANEL_BENCH opts into one delayed
// synthetic "20" trial per boot; the standard build remains manual-only.
#include <Arduino.h>
#include "../include/panel_mapping.h"

namespace {
constexpr uint8_t kData = 11, kClock = 12, kLatch = 10;
constexpr uint8_t kEnable = 9, kRowA = 4, kRowB = 5;
constexpr uint32_t kRowPeriodUs = 2000;
#ifdef DAYLIGHT_PANEL_BENCH
#ifndef BATTERY_PANEL_BENCH
#error "Daylight test requires the bounded standalone panel mode"
#endif
constexpr uint32_t kPulseUs = 200;
constexpr uint32_t kMapMs = 20000;
#else
constexpr uint32_t kPulseUs = 50;
constexpr uint32_t kMapMs = 10000;
#endif
constexpr uint32_t kTestMs = 3000;
bool offLevel = LOW;
bool blankBit = HIGH;
bool testing = false;
bool mapping = false;
bool digits = false;
uint32_t testDurationMs = kTestMs;
// -1 pulses the candidate active level; 0/1 holds ENABLE for polarity probes.
int forcedEnable = -1;
uint8_t row = 0;
uint32_t testStartedMs = 0, lastRowUs = 0, lastStatusMs = 0;
char command[32];
size_t commandSize = 0;
bool discardCommand = false;
#ifdef BATTERY_PANEL_BENCH
#ifdef DAYLIGHT_PANEL_BENCH
constexpr uint32_t kAutoDelayMs = 15000;
#else
constexpr uint32_t kAutoDelayMs = 5000;
#endif
bool autoPending = true;
uint32_t autoBaseMs = 0;
#endif

void report() {
  Serial.printf("PANEL_BENCH trial=1 test=%u oe_off=%u blank_bit=%u pulse_us=%lu drive=%d pattern=%s\n",
                testing, offLevel, blankBit, static_cast<unsigned long>(kPulseUs), forcedEnable,
                digits ? "digits20" : mapping ? "map" : "sparse");
}

void execute() {
  command[commandSize] = '\0';
  if (discardCommand) {
    Serial.println("ERROR invalid_or_overlong_command");
  } else if (strcmp(command, "stop") == 0) {
#ifdef BATTERY_PANEL_BENCH
    autoPending = false;
#endif
    testing = false;
    mapping = false;
    digits = false;
    testDurationMs = kTestMs;
    forcedEnable = -1;
    digitalWrite(kEnable, offLevel);
    report();
  } else if (strcmp(command, "status") == 0) {
    report();
#ifdef BATTERY_PANEL_BENCH
  } else if (commandSize != 0) {
    // Keep standalone diagnosis fixed and bounded; no polarity probes/rearming.
    Serial.println("ERROR battery_test_accepts_only_stop_or_status");
#else
  } else if (strcmp(command, "test") == 0 || strcmp(command, "map") == 0 || strcmp(command, "digits") == 0 || strcmp(command, "probe-low") == 0 ||
             strcmp(command, "probe-high") == 0) {
    if (testing) Serial.println("ERROR test_already_running");
    else {
      testing = true;
      mapping = strcmp(command, "map") == 0;
      digits = strcmp(command, "digits") == 0;
      testDurationMs = (mapping || digits) ? kMapMs : kTestMs;
      forcedEnable = strcmp(command, "probe-low") == 0 ? LOW :
                     strcmp(command, "probe-high") == 0 ? HIGH : -1;
      testStartedMs = millis();
      Serial.printf("TEST_STARTED duration_ms=%lu pattern=%s drive=%d\n",
                    static_cast<unsigned long>(testDurationMs), digits ? "digits20" : mapping ? "map" : "sparse", forcedEnable);
    }
  } else if (commandSize != 0) {
    // Configure only with panel power OFF; firmware cannot sense its power.
    if (testing) Serial.println("ERROR stop_before_configuration");
    else {
      bool recognized = true;
      if (strcmp(command, "oe-off-low") == 0) offLevel = LOW;
      else if (strcmp(command, "oe-off-high") == 0) offLevel = HIGH;
      else if (strcmp(command, "blank-bit-0") == 0) blankBit = LOW;
      else if (strcmp(command, "blank-bit-1") == 0) blankBit = HIGH;
      else recognized = false;
      if (recognized) {
        digitalWrite(kEnable, offLevel);
        report();
      } else Serial.println("ERROR unknown_command");
    }
#endif
  }
  commandSize = 0;
  discardCommand = false;
}

void refreshRow() {
  // Continue row cycling even during the blanking trial: a wrong OE/data
  // hypothesis must not deliberately hold one row address indefinitely.
  digitalWrite(kEnable, testing && forcedEnable >= 0 ? forcedEnable : offLevel);
  digitalWrite(kRowA, row & 1);
  digitalWrite(kRowB, (row >> 1) & 1);
  for (unsigned bit = 0; bit < 128; ++bit) {
    const auto pixel = panel_mapping::fromSerialBit(row, static_cast<uint8_t>(bit));
    const bool marked = digits ? panel_mapping::testNumber20(pixel) :
        mapping ? panel_mapping::orientationMark(pixel) : bit == 63;
    const bool value = (testing && marked) ? !blankBit : blankBit;
    digitalWrite(kData, value);
    delayMicroseconds(1);
    digitalWrite(kClock, HIGH);
    delayMicroseconds(1);
    digitalWrite(kClock, LOW);
  }
  digitalWrite(kLatch, HIGH);
  delayMicroseconds(2);
  digitalWrite(kLatch, LOW);
  if (testing && forcedEnable < 0 && uint32_t(millis() - testStartedMs) < testDurationMs) {
    digitalWrite(kEnable, !offLevel);
    delayMicroseconds(kPulseUs);
    digitalWrite(kEnable, offLevel);
  }
  row = (row + 1) & 3;
}
}  // namespace

void setup() {
  // USB programming requires isolated headers. Standalone mode uses the
  // recorded shared battery supply and must never be combined with USB power.
  for (uint8_t pin : {kEnable, kData, kClock, kLatch, kRowA, kRowB}) {
    digitalWrite(pin, pin == kData ? blankBit : LOW);
    pinMode(pin, OUTPUT);
  }
  Serial.begin(115200);
  delay(500);
#ifdef BATTERY_PANEL_BENCH
  autoBaseMs = millis();
#ifdef DAYLIGHT_PANEL_BENCH
  Serial.println("DAYLIGHT_PANEL_BENCH v1 SYNTHETIC_FIXED_20 auto_delay_ms=15000 duration_ms=20000 pulse_us=200 no_radar=1");
#else
  Serial.println("BATTERY_PANEL_BENCH v1 SYNTHETIC_FIXED_20 auto_delay_ms=5000 duration_ms=10000 no_radar=1");
#endif
#else
  Serial.println("PANEL_BENCH v4 EXPERIMENTAL_ONE_PANEL no_auto_test=1");
#endif
  Serial.println("Blanking is a hypothesis until observed; remove panel power for unexpected illumination.");
  report();
}

void loop() {
#ifdef BATTERY_PANEL_BENCH
  if (autoPending && uint32_t(millis() - autoBaseMs) >= kAutoDelayMs) {
    autoPending = false;
    testing = true;
    digits = true;
    mapping = false;
    forcedEnable = -1;
    testDurationMs = kMapMs;
    testStartedMs = millis();
    Serial.printf("TEST_STARTED duration_ms=%lu pattern=digits20 source=SYNTHETIC drive=-1\n",
                  static_cast<unsigned long>(testDurationMs));
  }
#endif
  if (testing && millis() - testStartedMs >= testDurationMs) {
    testing = false;
    mapping = false;
    digits = false;
    testDurationMs = kTestMs;
    forcedEnable = -1;
    digitalWrite(kEnable, offLevel);
    Serial.println("TEST_ENDED returning_to_blanking_trial");
  }
  if (micros() - lastRowUs >= kRowPeriodUs) {
    lastRowUs = micros();
    refreshRow();
  }
  for (unsigned n = 0; n < 32 && Serial.available(); ++n) {
    const int value = Serial.read();
    if (value == '\r' || value == '\n') execute();
    else if (value < 32 || value > 126) discardCommand = true;
    else if (!discardCommand) {
      if (commandSize < sizeof(command) - 1) command[commandSize++] = value;
      else discardCommand = true;
    }
  }
  if (millis() - lastStatusMs >= 5000) {
    lastStatusMs = millis();
    report();
  }
  delay(1);
}
