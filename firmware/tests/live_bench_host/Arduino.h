#pragma once
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <initializer_list>
#include <deque>
#include <string>
#include <vector>
#define LOW 0
#define HIGH 1
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
inline uint64_t fakeUs = 0;
inline int pins[64]{};
inline unsigned shifted = 0, ones = 0;
struct Frame { unsigned phase, bits, ones; uint64_t at; };
inline std::vector<Frame> frames;
inline std::vector<uint64_t> riseTimes, pulseWidths;
inline void digitalWrite(unsigned pin, int value) {
  const bool high = value != 0;
  if (pin == 12 && high && !pins[pin]) { ++shifted; ones += pins[11] != 0; }
  if (pin == 10 && high && !pins[pin]) {
    frames.push_back({unsigned(pins[4] | (pins[5] << 1)), shifted, ones, fakeUs});
    shifted = ones = 0;
  }
  if (pin == 9 && high && !pins[pin]) riseTimes.push_back(fakeUs);
  if (pin == 9 && !high && pins[pin]) pulseWidths.push_back(fakeUs - riseTimes.back());
  pins[pin] = high;
}
inline int modes[64]{};
inline void pinMode(unsigned pin, int mode) { modes[pin]=mode; }
inline uint32_t micros() { return static_cast<uint32_t>(fakeUs); }
inline uint32_t millis() { return static_cast<uint32_t>(fakeUs / 1000); }
inline void delay(unsigned ms) { fakeUs += ms * 1000ULL; }
inline void delayMicroseconds(unsigned us) { fakeUs += us; }
struct SerialStub {
  std::deque<char> input;
  std::string output;
  void begin(int) {}
  int available() { return input.size(); }
  int read() { if (input.empty()) return -1; const char c = input.front(); input.pop_front(); return c; }
  void println(const char* text) { output += text; output += '\n'; }
  void printf(const char* format, ...) {
    char buffer[512]; va_list args; va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); va_end(args); output += buffer;
  }
  void inject(const char* text) { while (*text) input.push_back(*text++); }
};
inline SerialStub Serial;

#include <functional>
enum hardwareSerial_error_t { UART_ERROR_TEST };
#define SERIAL_8N1 0
struct HardwareSerial {
  std::deque<uint8_t> input;
  std::function<void(hardwareSerial_error_t)> error;
  int rx=-1,tx=-1;
  explicit HardwareSerial(int) {}
  void setRxBufferSize(size_t) {}
  void onReceiveError(std::function<void(hardwareSerial_error_t)> cb) {error=cb;}
  void begin(int,int,int r,int t) {rx=r;tx=t;}
  int available() {return input.size();}
  int read() {if(input.empty())return -1;int b=input.front();input.pop_front();return b;}
};
