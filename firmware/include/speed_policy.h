#pragma once
#include "radar_reports.h"
namespace speed_policy {
// Conservative bench timeout, not the radar's configurable no-target delay.
constexpr uint32_t kExpiryMs=750;
class State {
 public:
  void clear() { valid_=false; kmh_=0; }
  void update(const radar_reports::Report& report,uint32_t now) {
    clear();
    // Raw 1 is provisionally toward, supported by toward-only capture and
    // approaching-only settings readback. No identity or lane claim is made.
    for (unsigned i=0;i<report.count;++i) {
      const auto& t=report.targets[i];
      if (t.direction==1 && (!valid_ || t.speedKmh>kmh_)) {
        valid_=true; kmh_=t.speedKmh;
      }
    }
    received_=now;
  }
  bool fresh(uint32_t now) const { return valid_ && uint32_t(now-received_)<kExpiryMs; }
  uint8_t mph() const { return uint8_t((uint32_t(kmh_)*1000000+804672)/1609344); }
  uint8_t kmh() const { return kmh_; }
  uint32_t age(uint32_t now) const { return uint32_t(now-received_); }
 private:
  bool valid_=false; uint8_t kmh_=0; uint32_t received_=0;
};
} // namespace speed_policy
