#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp32s3/spiram.h>

// USB COM bridge / UART0 only. No panel, radar, LED, Wi-Fi, or Bluetooth driver.
namespace {
constexpr size_t kMiB = 1024U * 1024U;
constexpr size_t kExpectedFlash = 16U * kMiB;
constexpr size_t kExpectedPsram = 8U * kMiB;
constexpr size_t kTestBytes = 7U * kMiB;  // Leave space for runtime allocations.
constexpr uint32_t kCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr unsigned kPasses = 6;
bool passed = false;
unsigned runs = 0;
uint32_t lastHeartbeat = 0;

uint32_t pattern(size_t index, unsigned pass) {
  switch (pass) {
    case 0: return 0x00000000U;
    case 1: return 0xffffffffU;
    case 2: return 0xaaaaaaaaU;
    case 3: return 0x55555555U;
    case 4: return static_cast<uint32_t>(index) * 0x9e3779b1U ^ 0xa5c35a3cU;
    default: return ~(static_cast<uint32_t>(index) * 0x9e3779b1U ^ 0xa5c35a3cU);
  }
}

void printInfo() {
  Serial.printf("INFO firmware=diagnostic-v1 chip=%s revision=%u cores=%u cpu_mhz=%u\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("INFO sdk=%s reset_reason=%d flash_bytes=%u flash_hz=%u\n",
                ESP.getSdkVersion(), static_cast<int>(esp_reset_reason()),
                ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
  Serial.printf("INFO psram_found=%u psram_bytes=%u psram_heap_bytes=%u psram_free=%u largest_block=%u\n",
                psramFound(), psramFound() ? static_cast<unsigned>(esp_spiram_get_size()) : 0U,
                ESP.getPsramSize(), ESP.getFreePsram(),
                static_cast<unsigned>(heap_caps_get_largest_free_block(kCaps)));
}

void runDiagnostic() {
  passed = false;
  ++runs;
  Serial.printf("DIAG BEGIN run=%u\n", runs);
  printInfo();
  if (ESP.getFlashChipSize() != kExpectedFlash || !psramFound() ||
      esp_spiram_get_size() != kExpectedPsram) {
    Serial.println("DIAG FAIL reason=unexpected_memory_configuration");
    return;
  }

  void* allocation = heap_caps_malloc(kTestBytes, kCaps);
  if (!allocation) {
    Serial.printf("DIAG FAIL reason=psram_allocation requested_bytes=%u\n",
                  static_cast<unsigned>(kTestBytes));
    return;
  }
  volatile uint32_t* memory = static_cast<volatile uint32_t*>(allocation);
  constexpr size_t words = kTestBytes / sizeof(uint32_t);
  Serial.printf("TEST allocation_bytes=%u patterns=%u\n",
                static_cast<unsigned>(kTestBytes), kPasses);
  const uint32_t started = millis();
  bool allPatternsPassed = true;
  for (unsigned pass = 0; pass < kPasses; ++pass) {
    // Fill the entire allocation before reading it, to exercise external RAM
    // beyond cache capacity and detect address aliasing with the index patterns.
    for (size_t i = 0; i < words; ++i) {
      memory[i] = pattern(i, pass);
      if ((i & 0x3fffU) == 0) delay(1);
    }
    for (size_t i = 0; i < words; ++i) {
      const uint32_t actual = memory[i];
      const uint32_t expected = pattern(i, pass);
      if (actual != expected) {
        Serial.printf("TEST FAIL pattern=%u byte_offset=%u expected=%08lx actual=%08lx\n",
                      pass, static_cast<unsigned>(i * sizeof(uint32_t)),
                      static_cast<unsigned long>(expected), static_cast<unsigned long>(actual));
        allPatternsPassed = false;
        break;
      }
      if ((i & 0x3fffU) == 0) delay(1);
    }
    if (!allPatternsPassed) break;
    Serial.printf("TEST PASS pattern=%u bytes=%u\n", pass, static_cast<unsigned>(kTestBytes));
  }
  heap_caps_free(allocation);
  const bool heapOk = heap_caps_check_integrity(kCaps, true);
  passed = allPatternsPassed && heapOk;
  Serial.printf("DIAG %s run=%u tested_bytes=%u patterns=%u heap_ok=%u elapsed_ms=%lu\n",
                passed ? "PASS" : "FAIL", runs, static_cast<unsigned>(kTestBytes),
                kPasses, heapOk, static_cast<unsigned long>(millis() - started));
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\nSPEED-DETECTOR DIAGNOSTIC v1; USB ONLY; PERIPHERALS DISCONNECTED");
  Serial.println("Commands: r = repeat memory test; ? = report configuration");
  runDiagnostic();
}

void loop() {
  while (Serial.available()) {
    const int command = Serial.read();
    if (command == 'r') runDiagnostic();
    else if (command == '?') printInfo();
  }
  if (millis() - lastHeartbeat >= 1000) {
    lastHeartbeat = millis();
    Serial.printf("HEARTBEAT uptime_ms=%lu status=%s runs=%u free_heap=%u free_psram=%u\n",
                  static_cast<unsigned long>(millis()), passed ? "PASS" : "FAIL", runs,
                  ESP.getFreeHeap(), ESP.getFreePsram());
  }
  delay(10);
}
