// Passive bench transport only. Raw records are not validated speed readings.
#include <Arduino.h>
#include <atomic>

namespace {
HardwareSerial radar(1);
constexpr int kRadarRx = 17;
constexpr size_t kChunkSize = 32;
std::atomic<unsigned> uartErrors{0};
uint8_t pending[kChunkSize];
size_t pendingSize = 0;
uint32_t firstByteMs = 0;
uint32_t totalBytes = 0;
uint32_t lastStatusMs = 0;
uint32_t sequence = 0;

void status() {
  Serial.printf("STATUS ms=%lu bytes=%lu errors=%u pending=%u\n",
                static_cast<unsigned long>(millis()),
                static_cast<unsigned long>(totalBytes), uartErrors.load(),
                static_cast<unsigned>(pendingSize));
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  radar.setRxBufferSize(4096);
  radar.onReceiveError([](hardwareSerial_error_t) { ++uartErrors; });
  // Explicit RX with TX=-1 leaves UART1 TX unattached in Arduino-ESP32 2.0.17.
  // Never forward host input or send radar configuration commands.
  radar.begin(115200, SERIAL_8N1, kRadarRx, -1);
  pinMode(kRadarRx, INPUT_PULLUP);
  Serial.println("RADAR_RECEIVE v1 rx=17 tx=disabled baud=115200 framing=8N1 raw_only=1");
}

void loop() {
  while (pendingSize < kChunkSize && radar.available()) {
    int value = radar.read();
    if (value < 0) break;
    if (pendingSize == 0) firstByteMs = millis();
    pending[pendingSize++] = static_cast<uint8_t>(value);
    ++totalBytes;
  }

  if (pendingSize && (pendingSize == kChunkSize || millis() - firstByteMs >= 25)) {
    char record[128];
    int length = snprintf(record, sizeof(record), "RAW %lu %lu %u ",
                          static_cast<unsigned long>(sequence),
                          static_cast<unsigned long>(firstByteMs), uartErrors.load());
    constexpr char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < pendingSize; ++i) {
      record[length++] = hex[pending[i] >> 4];
      record[length++] = hex[pending[i] & 15];
    }
    record[length++] = '\n';
    // Keep draining UART between console writes; driver reports RX overflows.
    // Hex transport cannot sustain a full-rate continuous 115200-baud input.
    if (Serial.availableForWrite() >= length) {
      Serial.write(reinterpret_cast<uint8_t *>(record), length);
      pendingSize = 0;
      ++sequence;
    }
  }
  if (millis() - lastStatusMs >= 1000 && !pendingSize && !radar.available()) {
    lastStatusMs = millis();
    status();
  }
  while (Serial.available()) Serial.read();
  delay(1);
}
