// Synthetic software-only checks; no physical radar or panel validation.
#define BATTERY_BENCH
#include "../../src/warning_bench.cpp"
#include <iostream>
void sendSpeed() {
  for(uint8_t v:{0xf4,0xf3,0xf2,0xf1,7,0,1,1,128,2,1,3,100,0xf8,0xf7,0xf6,0xf5})
    radar.input.push_back(v);
}
int main(int argc,char** argv) {
#ifdef DAYLIGHT_RADAR_BENCH
  constexpr uint32_t expectedDelayMs=15000,expectedPulseUs=200;
#else
  constexpr uint32_t expectedDelayMs=5000,expectedPulseUs=50;
#endif
  const bool rollover=argc>1 && !strcmp(argv[1],"rollover");
  if(rollover)fakeUs=uint64_t(0xfffff000u)*1000;
  setup();assert(radar.tx==-1 && modes[18]==INPUT);
  if(argc>1 && !strcmp(argv[1],"stop")) {
    Serial.inject("stop\n");loop();
    while(uint32_t(millis()-autoStartBase)<expectedDelayMs+25000)loop();
    assert(!armed&&!autoStartPending&&riseTimes.empty());
  } else {
    sendSpeed();loop();assert(speed.fresh(millis())&&!armed);
    while(uint32_t(millis()-autoStartBase)<expectedDelayMs) {
      assert(!armed&&riseTimes.empty());loop();
    }
    // Buffered pre-arm data must also be discarded at the automatic start.
    sendSpeed();loop();
    assert(armed&&!demo&&!autoStartPending&&!speed.fresh(millis()));
    assert(uint32_t(started-autoStartBase)>=expectedDelayMs &&
           uint32_t(started-autoStartBase)<expectedDelayMs+10);
    assert(riseTimes.empty());
    sendSpeed();for(int i=0;i<10;++i)loop();
    assert(speed.mph()==2&&speed.fresh(millis())&&!riseTimes.empty());
    while(speed.fresh(millis()))loop();
    size_t pulses=riseTimes.size();for(int i=0;i<10;++i)loop();
    assert(riseTimes.size()==pulses);
    sendSpeed();loop();++uartErrors;loop();assert(!speed.fresh(millis())&&!pins[9]);
    // Fresh input through the deadline must never extend the trial.
    while(armed){sendSpeed();loop();}
    assert(uint32_t(millis()-started)>=kTrialMs&&!pins[9]);
    pulses=riseTimes.size();
    while(uint32_t(millis()-started)<kTrialMs+6000){sendSpeed();loop();}
    assert(!armed&&riseTimes.size()==pulses);
    for(auto width:pulseWidths)assert(width==expectedPulseUs);
    for(auto at:riseTimes)assert(uint32_t(uint32_t(at/1000)-started)<kTrialMs);
  }
  std::cout<<"PASS battery bench delayed live start, clearing, bounded output, no automatic restart/cancel\n";
}
