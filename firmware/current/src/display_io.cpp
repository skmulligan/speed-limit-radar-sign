// Static GPIO bench diagnostic. PANEL MUST BE DISCONNECTED.
// A low output is not a verified panel blanking state. No radar or display scan.
#include <Arduino.h>

namespace {
constexpr uint8_t kPins[] = {11, 12, 10, 9, 4, 5};
constexpr uint32_t kHoldMs = 60000;
uint8_t outputMask = 0;
uint32_t lastChangeMs = 0;
uint32_t lastStatusMs = 0;
char command[16];
size_t commandSize = 0;
bool overflow = false;

void status() {
  // This reports commanded levels, not measured output voltages.
  Serial.printf("DISPLAY_IO commanded_mask=0x%02X ms=%lu panel_must_be_disconnected=1\n",
                static_cast<unsigned>(outputMask),
                static_cast<unsigned long>(millis()));
}

void setMask(uint8_t mask) {
  // Clear first so a one-channel selection cannot overlap the previous channel.
  for (uint8_t pin : kPins) digitalWrite(pin, LOW);
  for (size_t i = 0; i < sizeof(kPins) / sizeof(kPins[0]); ++i) {
    if (mask & (1u << i)) digitalWrite(kPins[i], HIGH);
  }
  outputMask = mask;
  lastChangeMs = millis();
  status();
}

void execute() {
  command[commandSize] = '\0';
  if (overflow) {
    Serial.println("ERROR command_too_long");
  } else if (strcmp(command, "low") == 0) {
    setMask(0);
  } else if (strcmp(command, "high") == 0) {
    setMask(0x3f);
  } else if (commandSize == 1 && command[0] >= '1' && command[0] <= '6') {
    setMask(static_cast<uint8_t>(1u << (command[0] - '1')));
  } else if (strcmp(command, "?") == 0) {
    status();
    Serial.println("COMMANDS newline-terminated: low, high, 1..6 (one high), ?");
  } else if (commandSize != 0) {
    Serial.println("ERROR unknown_command");
  }
  commandSize = 0;
  overflow = false;
}
}  // namespace

void setup() {
  for (uint8_t pin : kPins) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  Serial.begin(115200);
  delay(500);
  Serial.println("DISPLAY_IO v1 STATIC_ONLY PANEL_DISCONNECTED");
  Serial.println("CHANNELS 1=GPIO11 2=GPIO12 3=GPIO10 4=GPIO9 5=GPIO4 6=GPIO5");
  Serial.println("High states expire after 60 seconds; low does not imply panel blank.");
  status();
}

void loop() {
  if (outputMask != 0 && millis() - lastChangeMs >= kHoldMs) {
    Serial.println("TIMEOUT returning_all_GPIO_low");
    setMask(0);
  }
  // Bound work per loop so a continuous serial stream cannot block the timeout.
  for (unsigned i = 0; i < 32 && Serial.available(); ++i) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r' || value == '\n') {
      execute();
    } else if (!overflow) {
      if (commandSize < sizeof(command) - 1) command[commandSize++] = value;
      else overflow = true;
    }
  }
  if (millis() - lastStatusMs >= 5000) {
    lastStatusMs = millis();
    status();
  }
  delay(1);
}
