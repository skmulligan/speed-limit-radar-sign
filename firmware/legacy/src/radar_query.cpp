// Attended read-only radar settings query. Battery/panel must remain off.
#include <Arduino.h>
#include <atomic>
#include "../include/radar_query.h"
namespace {
HardwareSerial radar(1);
std::atomic<unsigned> errors{0};
bool txAttached = false;
uint32_t bytes = 0, lastStatus = 0;
char command[24]; size_t commandSize = 0; bool discard = false;
void hexLine(const char* label, const uint8_t* data, size_t size) {
  Serial.print(label);
  for (size_t i = 0; i < size; ++i) Serial.printf("%02X", data[i]);
  Serial.println();
}
void send(const uint8_t* data, size_t size) {
  hexLine("TX ", data, size);
  radar.write(data, size);
}
void event(const char* message) { Serial.println(message); }
void ack(const uint8_t* data, size_t size) { hexLine("ACK ", data, size); }
radar_query::Engine engine(send, event, ack);
void receiver(bool transmit) {
  radar.end();
  pinMode(18, INPUT); // No idle-high TX into an unpowered radar.
  radar.begin(115200, SERIAL_8N1, 17, transmit ? 18 : -1);
  radar.onReceiveError([](hardwareSerial_error_t) { ++errors; });
  pinMode(17, INPUT_PULLUP);
  txAttached = transmit;
}
void status() {
  Serial.printf("QUERY_STATUS active=%u tx_attached=%u bytes=%lu errors=%u\n",
      engine.active(), txAttached, static_cast<unsigned long>(bytes), errors.load());
}
void execute() {
  command[commandSize] = 0;
  if (discard) Serial.println("ERROR invalid_command");
  else if (!strcmp(command,"status")) status();
  else if (!strcmp(command,"stop")) engine.cancel(millis());
  else if (!strcmp(command,"query")) {
    if (!engine.active()) receiver(true);
    engine.start(millis());
  } else if (commandSize) Serial.println("ERROR unknown_command");
  commandSize = 0; discard = false;
}
}
void setup() {
  pinMode(18, INPUT);
  Serial.begin(115200); delay(500);
  radar.setRxBufferSize(4096);
  receiver(false);
  Serial.println("RADAR_QUERY v1 rx=17 tx=18 no_auto_query=1 read_only=1");
  status();
}
void loop() {
  // Raw bytes retained even if a malformed response cannot be parsed.
  uint8_t raw[32]; size_t count = 0;
  while (count < sizeof(raw) && radar.available()) {
    int b = radar.read(); if (b < 0) break;
    raw[count++] = uint8_t(b); ++bytes;
  }
  if (count) {
    hexLine("RX ", raw, count);
    for (size_t i=0; i<count; ++i) engine.feed(raw[i], millis());
  }
  engine.tick(millis());
  if (txAttached && !engine.active()) {
    radar.flush(); receiver(false);
    Serial.println("TX_DETACHED");
  }
  for (unsigned n=0; n<32 && Serial.available(); ++n) {
    int b = Serial.read();
    if (b=='\r' || b=='\n') execute();
    else if (b<32 || b>126) discard=true;
    else if (!discard) {
      if (commandSize<sizeof(command)-1) command[commandSize++]=char(b);
      else discard=true;
    }
  }
  if (millis()-lastStatus>=5000) { lastStatus=millis(); status(); }
  delay(1);
}
