// Synthetic UART and clock inputs, not physical radar or endurance validation.
#define BATTERY_BENCH
#define DAYLIGHT_RADAR_BENCH
#define DAYLIGHT_STARTUP_CHECK
#define CONTINUOUS_RADAR
#include "../../src/warning_bench.cpp"
#include <iostream>

void send(uint8_t kmh=3) {
  for(uint8_t v:{0xf4,0xf3,0xf2,0xf1,7,0,1,1,128,2,1,int(kmh),100,0xf8,0xf7,0xf6,0xf5})
    radar.input.push_back(v);
}
void stopAndCheck() {
  Serial.inject("stop\n");loop();
  const auto count=riseTimes.size();
  for(unsigned i=0;i<100;++i){send();loop();}
  fakeUs+=60000000;loop();
  assert(!armed&&!startupCheck&&!autoStartPending&&!pins[9]);
  assert(riseTimes.size()==count);
}
int main(int argc,char** argv) {
  const std::string mode=argc>1 ? argv[1] : "normal";
  if(mode=="rollover")fakeUs=uint64_t(0xfffff000u)*1000;
  setup();assert(radar.rx==17&&radar.tx==-1&&modes[18]==INPUT);
  const uint64_t boot=fakeUs;
  if(mode=="stop-pending") {stop();loop();assert(riseTimes.empty());stopAndCheck();return 0;}
  loop();assert(startupCheck&&!armed&&fakeUs-boot<10000);
  const uint32_t markerStart=startupCheckStarted;
  // Independent full raster: STARTING in a single 31-pixel line at y=5.
  const char* expected[]={
    "###.###..#..##..###.###.#.#.###.",
    "#....#..#.#.#.#..#...#..###.#...",
    "###..#..###.##...#...#..###.#.#.",
    "..#..#..#.#.#.#..#...#..###.#.#.",
    "###..#..#.#.#.#..#..###.#.#.###."
  };
  unsigned phaseLit[4]{};
  for(uint8_t y=0;y<16;++y) {
    for(uint8_t x=0;x<32;++x) {
      const bool lit=y>=5&&y<10&&expected[y-5][x]=='#';
      assert(warning_text::starting({x,y})==lit);
      if(lit)++phaseLit[y%4];
      std::cout<<(lit?'#':'.');
    }
    std::cout<<'\n';
  }
  for(int i=0;i<30;++i){send();++uartErrors;loop();}
  Serial.inject("live\ndemo-warning\n");for(int i=0;i<4;++i)loop();
  assert(startupCheck&&!armed&&!demo);
  assert(Serial.output.find("ERROR trial_already_running")!=std::string::npos);
  if(mode=="stop-marker"){stopAndCheck();return 0;}
  while(uint32_t(millis()-markerStart)<2998){send();loop();}
  for(const auto& f:frames)assert(f.bits==128&&f.ones==128-phaseLit[f.phase]);
  while(startupCheck){send();loop();}
  assert(armed&&!demo&&!speed.fresh(millis())&&!pins[9]);
  assert(uint32_t(started-markerStart)>=3000&&uint32_t(started-markerStart)<3010);
  const auto markerPulses=riseTimes.size();
  for(int i=0;i<30;++i)loop();assert(riseTimes.size()==markerPulses);
  send();for(int i=0;i<10;++i)loop();assert(speed.mph()==2&&riseTimes.size()>markerPulses);
  if(mode=="stop-live"){stopAndCheck();return 0;}
  // Run through the old 20s deadline and a full minute, with fresh traffic.
  uint32_t lastSend=millis();
  while(uint32_t(millis()-started)<65000) {
    if(uint32_t(millis()-lastSend)>=100){send();lastSend=millis();}
    loop();assert(armed);
  }
  assert(speed.fresh(millis())&&Serial.output.find("WARNING_ENDED")==std::string::npos);
  auto pulses=riseTimes.size();
  // Sparse long-elapsed clock checks, not a two-hour hardware or simulation run.
  for(uint64_t jump:{3600000000ULL,7200000000ULL,4294967296000ULL}) {
    fakeUs+=jump;send();for(int i=0;i<10;++i)loop();
    assert(armed&&speed.fresh(millis())&&riseTimes.size()>pulses);pulses=riseTimes.size();
  }
  // Warning policy still runs above 20 MPH; fresh samples sustain its message.
  send(40);loop();assert(warning.warning());
  const uint32_t warningStart=millis();lastSend=millis();
  while(uint32_t(millis()-warningStart)<3100) {
    if(uint32_t(millis()-lastSend)>=100){send(40);lastSend=millis();}loop();
  }
  assert(currentView==warning_policy::View::Message);
  // Empty report, malformed byte, UART fault, backlog, and expiry blank output.
  for(int fault=0;fault<5;++fault) {
    send(40);for(int i=0;i<10;++i)loop();assert(speed.fresh(millis()));
    if(fault==0)for(uint8_t v:{0xf4,0xf3,0xf2,0xf1,2,0,0,0,0xf8,0xf7,0xf6,0xf5})radar.input.push_back(v);
    if(fault==1)radar.input.insert(radar.input.end(),4,0);
    if(fault==2)++uartErrors;
    if(fault==3)radar.input.insert(radar.input.end(),300,0);
    if(fault==4)fakeUs+=750000;
    loop();assert(!speed.fresh(millis())&&!warning.warning()&&!pins[9]&&armed);
    pulses=riseTimes.size();for(int i=0;i<10;++i)loop();assert(riseTimes.size()==pulses);
  }
  // An expired sample must not revive when millis returns to its old timestamp.
  send();loop();const uint64_t received=fakeUs;
  fakeUs+=800000;loop();assert(!speed.fresh(millis()));
  pulses=riseTimes.size();fakeUs=received+4294967296000ULL;
  for(int i=0;i<10;++i)loop();assert(!speed.fresh(millis())&&riseTimes.size()==pulses);
  for(auto width:pulseWidths)assert(width==200);
  stopAndCheck();
  // Explicit synthetic demos remain bounded, even in this build.
  Serial.inject("demo-warning\n");loop();assert(armed&&demo);
  fakeUs+=20000000;loop();assert(!armed&&!demo&&!pins[9]);
  std::cout<<"PASS STARTING raster, immediate boot, continuous live beyond deadline, long elapsed/rollover, stale/fault clearing, warning, stop and bounded demo\n";
}
