#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// LD2451 V1.03 report envelope/fields; local capacity is 16 targets, not a
// manufacturer limit. Captured no-target/walking fixtures verify observed forms.
namespace radar_reports {
struct Target { uint8_t angle, distance, direction, speedKmh, snr; };
struct Report { uint8_t count = 0, alarm = 0; Target targets[16]{}; };
class Parser {
 public:
  using Frame = void (*)(const Report&, uint32_t);
  using Invalid = void (*)();
  Parser(Frame frame, Invalid invalid) : frame_(frame), invalid_(invalid) {}
  void reset() { used_ = 0; }
  void feed(uint8_t byte, uint32_t now) {
    if (used_ == sizeof(buffer_)) { drop(1); invalid_(); }
    buffer_[used_++] = byte;
    while (used_ >= 4) {
      const uint8_t head[] = {0xf4,0xf3,0xf2,0xf1};
      if (memcmp(buffer_, head, 4)) { drop(1); invalid_(); continue; }
      if (used_ < 6) return;
      const size_t length = buffer_[4] | (size_t(buffer_[5]) << 8);
      if (length > 82 || (length != 0 && (length < 2 || (length-2)%5))) {
        drop(1); invalid_(); continue;
      }
      if (used_ < length+10) return;
      const uint8_t tail[] = {0xf8,0xf7,0xf6,0xf5};
      if (memcmp(buffer_+6+length, tail, 4)) { drop(1); invalid_(); continue; }
      Report report;
      bool valid = true;
      if (length) {
        report.count = buffer_[6]; report.alarm = buffer_[7];
        valid = report.count <= 16 && length == 2+5u*report.count && report.alarm <= 1;
        if (valid) for (unsigned i=0;i<report.count;++i) {
          const uint8_t* p=buffer_+8+5*i;
          report.targets[i]={p[0],p[1],p[2],p[3],p[4]};
          if (p[1]>100 || p[2]>1 || p[3]>120) valid=false;
        }
      }
      drop(length+10);
      if (valid) frame_(report,now); else invalid_();
    }
  }
 private:
  Frame frame_; Invalid invalid_;
  uint8_t buffer_[92]{}; size_t used_=0;
  void drop(size_t n) { used_-=n; memmove(buffer_,buffer_+n,used_); }
};
} // namespace radar_reports
