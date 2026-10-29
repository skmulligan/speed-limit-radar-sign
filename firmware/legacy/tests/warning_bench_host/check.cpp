// Entire fixture is synthetic. This is not a physical panel/radar validation.
#include "../../src/warning_bench.cpp"
#include <iostream>
#include <limits>
using warning_policy::View;
radar_reports::Report sample(uint8_t kmh) {
  radar_reports::Report r;r.count=1;r.targets[0]={128,2,1,kmh,100};return r;
}
void tickTo(uint32_t elapsed) {while(uint32_t(millis()-started)<elapsed)loop();}
void send(uint8_t kmh) {
  for(uint8_t v:{0xf4,0xf3,0xf2,0xf1,7,0,1,1,128,2,1,int(kmh),100,0xf8,0xf7,0xf6,0xf5})radar.input.push_back(v);
}
void commandLine(const char* text) {Serial.inject(text);loop();}
int main() {
  speed_policy::State s;warning_policy::State w;
  s.update(sample(32),0);assert(w.view(s,0)==View::Number&&!w.warning()); // 19.88 MPH
  s.update(sample(33),1);assert(w.view(s,1)==View::Number&&w.warning()); // 20.51 MPH
  s.update(sample(31),2);assert(w.view(s,2)==View::Number&&w.warning()); // 19.26 MPH held
  s.update(sample(30),3);assert(w.view(s,3)==View::Number&&!w.warning()); // 18.64 MPH clears
  // Fresh reports maintain the original phase; warning never freezes the speed.
  const uint32_t base=100;
  s.update(sample(40),base);assert(w.view(s,base)==View::Number);
  for(auto entry:std::vector<std::pair<uint32_t,View>>{{999,View::Number},{1000,View::Number},
      {1249,View::Number},{1250,View::Blank},{1500,View::Number},{2999,View::Blank},
      {3000,View::Message},{4999,View::Message},{5000,View::Number}}) {
    s.update(sample(48),base+entry.first);assert(w.view(s,base+entry.first)==entry.second);
    assert(s.mph()==30);
  }
  // Each phase is interrupted immediately by an empty/invalid sample, and by expiry.
  for(uint32_t offset:{0u,1250u,3000u}) {
    w.reset();s.update(sample(40),0);w.view(s,0);
    s.update(sample(40),offset);w.view(s,offset);s.clear();
    assert(w.view(s,offset)==View::Blank&&!w.warning());
    s.update(sample(40),offset+1);assert(w.view(s,offset+1)==View::Number&&w.warning());
    assert(w.view(s,offset+751)==View::Blank&&!w.warning());
    s.update(sample(40),offset+752);assert(w.view(s,offset+752)==View::Number);
    s.update(radar_reports::Report{},offset+753);assert(w.view(s,offset+753)==View::Blank);
  }
  w.reset();uint32_t wrap=0xfffffc00u;s.update(sample(40),wrap);w.view(s,wrap);
  s.update(sample(40),uint32_t(wrap+3000));assert(w.view(s,uint32_t(wrap+3000))==View::Message);
  assert(w.view(s,uint32_t(wrap+3750))==View::Blank);
  // Print the exact raster for visual review. All content fits the 32 x 16 panel.
  for(uint8_t y=0;y<16;++y){for(uint8_t x=0;x<32;++x){
    bool on=warning_text::slowDown({x,y});
    if(on)assert(x>=4&&x<=26&&y!=7&&y<15);
    assert(!(on&&warning_text::testMarker({x,y})));
    std::cout<<(on?'#':'.');
  }std::cout<<'\n';}
  // Real firmware loop: no illumination at boot even with incoming speed data.
  setup();assert(modes[18]==INPUT&&radar.tx==-1&&radar.rx==17);
  send(40);for(int i=0;i<20;++i)loop();assert(!armed&&riseTimes.empty());
  for(const auto& f:frames)assert(f.ones==128);
  commandLine("demo-warning\n");assert(armed&&demo);
  const uint32_t start=started;
  tickTo(1100);assert(speed.mph()==18);
  // Real UART reports/errors cannot contaminate an explicitly synthetic trial.
  send(100);loop();assert(speed.mph()==18);
  ++uartErrors;loop();assert(speed.mph()==18);
  tickTo(4100);assert(speed.mph()==25&&warning.warning());
  commandLine("live\n");assert(demo&&started==start);
  tickTo(7200);assert(currentView==View::Message);
  tickTo(12100);assert(currentView==View::Message&&speed.fresh(millis()));
  tickTo(12800);assert(!speed.fresh(millis())&&!warning.warning()&&currentView==View::Blank);
  size_t pulses=riseTimes.size();tickTo(13900);assert(riseTimes.size()==pulses);
  tickTo(14150);assert(speed.mph()==18&&speed.fresh(millis())&&!warning.warning());
  tickTo(17150);assert(!speed.fresh(millis())&&currentView==View::Blank);
  tickTo(20020);assert(!armed&&!demo&&!pins[9]);
  for(auto width:pulseWidths)assert(width==50);
  for(auto at:riseTimes)assert(at/1000-start<20000);
  assert(Serial.output.find("WARNING_ENDED")!=std::string::npos);
  // Switching to live starts empty; faults/staleness cannot leave a warning lit.
  commandLine("live\n");assert(armed&&!demo&&!speed.fresh(millis()));
  pulses=riseTimes.size();for(int i=0;i<15;++i)loop();assert(riseTimes.size()==pulses);
  send(40);for(int i=0;i<10;++i)loop();assert(speed.mph()==25&&warning.warning());
  ++uartErrors;loop();assert(!speed.fresh(millis())&&!warning.warning()&&!pins[9]);
  send(40);loop();radar.input.insert(radar.input.end(),300,0);loop();assert(!speed.fresh(millis()));
  send(40);loop();invalid();assert(!speed.fresh(millis())&&!pins[9]);
  send(40);loop();while(speed.fresh(millis()))loop();
  pulses=riseTimes.size();for(int i=0;i<10;++i)loop();assert(riseTimes.size()==pulses&&!warning.warning());
  commandLine("stop\n");assert(!armed&&!speed.fresh(millis()));
  commandLine("demo-warning\n");tickTo(5000);commandLine("stop\n");
  assert(!armed&&!demo&&!speed.fresh(millis())&&!pins[9]);
  std::cout<<"PASS warning thresholds, hysteresis, cadence, stale clearing, rollover, synthetic isolation, bounded real-loop trials and GPIO timing\n";
}
