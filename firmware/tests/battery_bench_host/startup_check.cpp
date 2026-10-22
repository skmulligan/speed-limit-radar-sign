// Synthetic software simulation, not physical radar/display validation.
#define BATTERY_BENCH
#define DAYLIGHT_RADAR_BENCH
#define DAYLIGHT_STARTUP_CHECK
#include "../../src/warning_bench.cpp"
#include <iostream>

void sendSpeed() {
  for(uint8_t v:{0xf4,0xf3,0xf2,0xf1,7,0,1,1,128,2,1,3,100,0xf8,0xf7,0xf6,0xf5})
    radar.input.push_back(v);
}
void assertStaysDark() {
  const auto pulses=riseTimes.size();
  const uint64_t until=fakeUs+45000000;
  while(fakeUs<until){sendSpeed();loop();}
  assert(!armed&&!startupCheck&&!autoStartPending&&!pins[9]);
  assert(riseTimes.size()==pulses);
}
int main(int argc,char** argv) {
  const std::string mode=argc>1 ? argv[1] : "normal";
  if(mode=="rollover")fakeUs=uint64_t(0xfffff000u)*1000;
  setup();assert(radar.rx==17 && radar.tx==-1 && modes[18]==INPUT);
  const uint64_t base=fakeUs;
  if(mode=="stop-pending") {
    Serial.inject("stop\n");loop();assertStaysDark();
    assert(riseTimes.empty());return 0;
  }
  while(fakeUs-base<14998000){sendSpeed();loop();assert(!startupCheck&&!armed&&riseTimes.empty());}
  while(!startupCheck){assert(fakeUs-base<15010000);loop();}
  assert(!armed&&!demo&&!speed.fresh(millis()));
  const uint32_t checkStart=startupCheckStarted;
  const size_t markerFrameStart=frames.size();
  // Even valid samples, UART faults, and repeat commands cannot replace TEST.
  for(int i=0;i<100;++i){sendSpeed();++uartErrors;loop();}
  Serial.inject("live\ndemo-warning\n");for(int i=0;i<5;++i)loop();
  assert(startupCheck&&!armed&&!demo);
  assert(Serial.output.find("ERROR trial_already_running")!=std::string::npos);
  if(mode=="stop-marker") {
    Serial.inject("stop\n");loop();assertStaysDark();return 0;
  }
  while(uint32_t(millis()-checkStart)<2998){sendSpeed();loop();}
  // Capture pattern counts before transition. Independent TEST row raster
  // totals at top=4: 19,4,4,9,4,4,11 pixels -> phases 23,8,15,9.
  const unsigned expectedLit[]={23,8,15,9};
  bool phaseSeen[4]{};
  for(size_t i=markerFrameStart;i<frames.size();++i) {
    const auto& f=frames[i];
    assert(f.bits==128 && f.ones==128-expectedLit[f.phase]);
    phaseSeen[f.phase]=true;
  }
  for(bool seen:phaseSeen)assert(seen);
  // Queue data up to transition; live arming must discard any old sample.
  while(startupCheck){sendSpeed();loop();}
  assert(armed&&!demo&&!speed.fresh(millis())&&!pins[9]);
  assert(uint32_t(started-checkStart)>=3000 && uint32_t(started-checkStart)<3010);
  const size_t markerPulses=riseTimes.size();
  for(int i=0;i<50;++i)loop();
  assert(riseTimes.size()==markerPulses); // No fake speed after TEST.
  sendSpeed();for(int i=0;i<10;++i)loop();
  assert(speed.fresh(millis())&&speed.mph()==2&&riseTimes.size()>markerPulses);
  if(mode=="stop-live") {
    Serial.inject("stop\n");loop();assertStaysDark();return 0;
  }
  while(speed.fresh(millis()))loop();
  size_t pulses=riseTimes.size();for(int i=0;i<20;++i)loop();assert(riseTimes.size()==pulses);
  sendSpeed();loop();++uartErrors;loop();assert(!speed.fresh(millis())&&!pins[9]);
  while(armed){sendSpeed();loop();}
  assert(uint32_t(millis()-started)>=20000 && uint32_t(millis()-started)<20010);
  for(auto width:pulseWidths)assert(width==200);
  for(size_t i=0;i<riseTimes.size();++i) {
    const uint32_t at=uint32_t(riseTimes[i]/1000);
    if(i<markerPulses)assert(uint32_t(at-checkStart)<3000);
    else assert(uint32_t(at-started)<20000);
  }
  assertStaysDark();
  std::cout<<"PASS synthetic TEST raster/deadline, real-only live transition, freshness/error clearing, 200-us pulses and no restart\n";
}
