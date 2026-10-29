#pragma once
#include "speed_policy.h"
namespace warning_policy {
enum class View { Blank, Number, Message };
struct Config {
  uint8_t limitMph=20;
  uint8_t hysteresisMph=1;
  uint32_t numberMs=1000, flashMs=2000, messageMs=2000, halfFlashMs=250;
};
class State {
 public:
  explicit State(Config config=Config()) : config_(config) {}
  void reset() { warning_=false; }
  bool warning() const { return warning_; }
  View view(const speed_policy::State& speed,uint32_t now) {
    if(!speed.fresh(now)) {reset();return View::Blank;}
    const uint32_t scaled=uint32_t(speed.kmh())*1000000u;
    const uint32_t limit=uint32_t(config_.limitMph)*1609344u;
    const uint8_t release=config_.limitMph>config_.hysteresisMph ? config_.limitMph-config_.hysteresisMph : 0;
    if(!warning_ && scaled>limit) {warning_=true;started_=now;}
    else if(warning_ && scaled<=uint32_t(release)*1609344u) reset();
    if(!warning_)return View::Number;
    const uint32_t cycle=config_.numberMs+config_.flashMs+config_.messageMs;
    if(!cycle)return View::Number;
    uint32_t phase=uint32_t(now-started_)%cycle;
    if(phase<config_.numberMs)return View::Number;
    phase-=config_.numberMs;
    if(phase<config_.flashMs)
      return !config_.halfFlashMs || (phase/config_.halfFlashMs)%2==0 ? View::Number : View::Blank;
    return View::Message;
  }
 private:
  Config config_; bool warning_=false; uint32_t started_=0;
};
} // namespace warning_policy
