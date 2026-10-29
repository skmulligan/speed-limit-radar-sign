#include "../../src/live_bench.cpp"
#include <fstream>
#include <iterator>
#include <iostream>
using Bytes=std::vector<uint8_t>;
std::vector<radar_reports::Report> parsed;
unsigned badCount=0;
void collect(const radar_reports::Report& r,uint32_t){parsed.push_back(r);}
void bad(){++badCount;}
Bytes packet(Bytes payload){Bytes b={0xf4,0xf3,0xf2,0xf1,uint8_t(payload.size()),0};b.insert(b.end(),payload.begin(),payload.end());b.insert(b.end(),{0xf8,0xf7,0xf6,0xf5});return b;}
void inject(const Bytes& b){for(auto v:b)radar.input.push_back(v);}
void consume(radar_reports::Parser& p,const Bytes& b,uint32_t now=0){for(auto v:b)p.feed(v,now);}
int main(){
  // Actual captured bytes, with provenance in companion Markdown files.
  const char* names[]={"no-target","walking-01","toward-01","away-01"};
  const unsigned expected[]={14,24,14,14},targets[]={0,6,2,0};
  for(unsigned i=0;i<4;++i){
    parsed.clear();badCount=0;radar_reports::Parser p(collect,bad);
    std::ifstream file(std::string("tests/fixtures/ld2451/2026-10-17-captured-")+names[i]+".bin",std::ios::binary);
    assert(file.good());Bytes data((std::istreambuf_iterator<char>(file)),{});
    consume(p,data);assert(parsed.size()==expected[i] && badCount==0);
    unsigned n=0;for(auto& r:parsed)n+=r.count;assert(n==targets[i]);
  }
  // Synthetic fragmented/combined reports, invalid fields and resync.
  radar_reports::Parser p(collect,bad);parsed.clear();badCount=0;
  Bytes good=packet({1,1,128,2,1,32,80});
  consume(p,Bytes(good.begin(),good.begin()+7));assert(parsed.empty());
  consume(p,Bytes(good.begin()+7,good.end()));assert(parsed.size()==1);
  Bytes malformed=good;malformed.back()=0;consume(p,malformed);consume(p,good);
  assert(parsed.size()==2 && badCount>0);
  consume(p,{0xf4,0xf3,0xf2,0xf1,255,255});consume(p,good);assert(parsed.size()==3);
  for(auto payload:std::vector<Bytes>{{2,1,128,2,1,3,80},{1,1,128,2,2,3,80},
      {1,1,128,2,1,121,80},{1,1,128,101,1,3,80},{1,2,128,2,1,3,80}}){
    size_t before=parsed.size();consume(p,packet(payload));assert(parsed.size()==before);
  }
  // Select fastest eligible approach; receding/empty must clear immediately.
  speed_policy::State policy;radar_reports::Report r;
  r.count=3;r.targets[0]={128,4,1,20,50};r.targets[1]={128,8,1,32,50};r.targets[2]={128,2,0,100,50};
  policy.update(r,100);assert(policy.fresh(849)&&!policy.fresh(850)&&policy.mph()==20);
  r.count=1;r.targets[0].direction=0;policy.update(r,200);assert(!policy.fresh(200));
  r.count=0;policy.update(r,300);assert(!policy.fresh(300));
  r.count=1;r.targets[0]={128,2,1,3,80};policy.update(r,0xfffffff0u);
  assert(policy.fresh(uint32_t(0xfffffff0u+749u)));assert(!policy.fresh(uint32_t(0xfffffff0u+750u)));
  // Dynamic rendering of 20 must exactly match the bench-validated raster.
  for(uint8_t y=0;y<16;++y)for(uint8_t x=0;x<32;++x)
    assert(speed_digits::pixel(20,{x,y})==panel_mapping::testNumber20({x,y}));
  for(unsigned mph=0;mph<=75;++mph){unsigned lit=0;
    for(uint8_t y=0;y<16;++y)for(uint8_t x=0;x<32;++x){bool on=speed_digits::pixel(mph,{x,y});lit+=on;
      if(on)assert(y>=1&&y<=14&&x>=5&&x<=26);}
    assert(lit>0 && lit<=200);
  }
  // Exercise the real firmware loop with fake GPIO/time/UART.
  setup();assert(modes[18]==INPUT&&radar.tx==-1&&radar.rx==17);
  inject(good);for(int i=0;i<20;++i)loop();assert(!armed&&riseTimes.empty());
  for(auto& f:frames)assert(f.ones==128);
  Serial.inject("live\n");loop();assert(armed&&!speed.fresh(millis()));
  uint64_t began=fakeUs;
  inject(good);for(int i=0;i<15;++i)loop();assert(speed.fresh(millis())&&speed.mph()==20&&!riseTimes.empty());
  for(auto width:pulseWidths)assert(width==50);
  while(speed.fresh(millis()))loop();size_t pulses=riseTimes.size();
  for(int i=0;i<10;++i)loop();assert(riseTimes.size()==pulses);
  inject(good);loop();assert(speed.fresh(millis()));
  inject(packet({}));loop();assert(!speed.fresh(millis())&&!pins[9]);
  inject(good);loop();assert(speed.fresh(millis()));
  inject(Bytes(300,0));loop();assert(!speed.fresh(millis())&&backlogs==1);
  inject(good);loop();++uartErrors;loop();assert(!speed.fresh(millis())&&backlogs==2);
  Serial.inject("live\n");loop();assert(Serial.output.find("ERROR trial_already_running")!=std::string::npos);
  while(armed)loop();assert(fakeUs-began<20010000&&!pins[9]);
  Serial.inject("live\n");loop();inject(good);loop();Serial.inject("stop\n");loop();assert(!armed&&!speed.fresh(millis())&&!pins[9]);
  Serial.inject("live12345678901234567890123456789012345\n");for(int i=0;i<3;++i)loop();assert(!armed);
  std::cout<<"PASS captured and synthetic parser/policy/real-loop tests: freshness, no-target clearing, bounded trial, low-duty GPIO, RX faults, digits\n";
}
