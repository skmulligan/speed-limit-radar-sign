// Attended one-panel radar-to-MPH bench trial. No automatic illumination,
// no radar TX/configuration, no warning sequence, and no outdoor-ready claim.
#include <Arduino.h>
#include <atomic>
#include "../include/radar_reports.h"
#include "../include/speed_policy.h"
#include "../include/speed_digits.h"
namespace {
constexpr uint8_t kData=11,kClock=12,kLatch=10,kEnable=9,kRowA=4,kRowB=5;
constexpr uint32_t kTrialMs=20000,kRowUs=2000,kPulseUs=50;
HardwareSerial radar(1);
std::atomic<unsigned> uartErrors{0};
unsigned checkedErrors=0,invalidBytes=0,reportFrames=0,backlogs=0;
speed_policy::State speed;
bool armed=false; uint32_t started=0,lastRow=0,lastStatus=0;
uint8_t row=0;
char command[24]; size_t commandSize=0; bool discard=false;
void report(const radar_reports::Report& r,uint32_t now) {
  ++reportFrames; speed.update(r,now);
  if(speed.fresh(now)) Serial.printf("SAMPLE targets=%u kmh=%u mph=%u direction=1\n",r.count,speed.kmh(),speed.mph());
  else Serial.printf("NO_TARGET eligible=0 targets=%u\n",r.count);
}
void invalid() { ++invalidBytes; speed.clear(); digitalWrite(kEnable,LOW); }
radar_reports::Parser parser(report,invalid);
void status() {
  Serial.printf("LIVE_STATUS armed=%u fresh=%u mph=%d frames=%u invalid=%u errors=%u backlogs=%u\n",
    armed,speed.fresh(millis()),speed.fresh(millis()) ? int(speed.mph()) : -1,
    reportFrames,invalidBytes,uartErrors.load(),backlogs);
}
void stop() { armed=false;speed.clear();digitalWrite(kEnable,LOW); }
void execute() {
  command[commandSize]=0;
  if(discard) Serial.println("ERROR invalid_command");
  else if(!strcmp(command,"status")) status();
  else if(!strcmp(command,"stop")) { stop();Serial.println("LIVE_STOPPED"); }
  else if(!strcmp(command,"live")) {
    if(armed) Serial.println("ERROR trial_already_running");
    else {
      // Never reuse a sample or partial packet from before explicit arming.
      for(unsigned n=0;n<4096 && radar.available();++n) radar.read();
      parser.reset();speed.clear();started=millis();armed=true;
      Serial.println("LIVE_STARTED duration_ms=20000 units=MPH expiry_ms=750");
    }
  } else if(commandSize) Serial.println("ERROR unknown_command");
  commandSize=0;discard=false;
}
void refresh() {
  digitalWrite(kEnable,LOW);
  digitalWrite(kRowA,row&1);digitalWrite(kRowB,(row>>1)&1);
  const bool show=armed && speed.fresh(millis());
  const uint8_t mph=speed.mph();
  for(unsigned bit=0;bit<128;++bit) {
    bool lit=show && speed_digits::pixel(mph,panel_mapping::fromSerialBit(row,uint8_t(bit)));
    digitalWrite(kData,!lit);delayMicroseconds(1);
    digitalWrite(kClock,HIGH);delayMicroseconds(1);digitalWrite(kClock,LOW);
  }
  digitalWrite(kLatch,HIGH);delayMicroseconds(2);digitalWrite(kLatch,LOW);
  // Recheck deadlines after shifting; stale data must not get a new pulse.
  if(show && speed.fresh(millis()) && uint32_t(millis()-started)<kTrialMs && uartErrors.load()==checkedErrors) {
    digitalWrite(kEnable,HIGH);delayMicroseconds(kPulseUs);digitalWrite(kEnable,LOW);
  }
  row=(row+1)&3;
}
}
void setup() {
  for(uint8_t pin:{kEnable,kData,kClock,kLatch,kRowA,kRowB}) {
    digitalWrite(pin,pin==kData ? HIGH:LOW);pinMode(pin,OUTPUT);
  }
  pinMode(18,INPUT); // Radar RX remains connected but is never driven here.
  Serial.begin(115200);delay(500);
  radar.setRxBufferSize(4096);
  radar.onReceiveError([](hardwareSerial_error_t){++uartErrors;});
  radar.begin(115200,SERIAL_8N1,17,-1);pinMode(17,INPUT_PULLUP);
  Serial.println("LIVE_BENCH v1 no_auto_display=1 rx=17 tx=disabled units=MPH");
  status();
}
void loop() {
  if(armed && uint32_t(millis()-started)>=kTrialMs) {stop();Serial.println("LIVE_ENDED");}
  const unsigned observed=uartErrors.load();
  if(observed!=checkedErrors || radar.available()>256) {
    checkedErrors=observed;++backlogs;speed.clear();parser.reset();digitalWrite(kEnable,LOW);
    for(unsigned n=0;n<4096 && radar.available();++n)radar.read();
    Serial.println("INPUT_DISCARDED uart_error_or_backlog");
  }
  for(unsigned n=0;n<256 && radar.available();++n) {
    int value=radar.read();if(value<0)break;parser.feed(uint8_t(value),millis());
  }
  for(unsigned n=0;n<32 && Serial.available();++n) {
    int b=Serial.read();
    if(b=='\r'||b=='\n')execute();
    else if(b<32||b>126)discard=true;
    else if(!discard) {
      if(commandSize<sizeof(command)-1)command[commandSize++]=char(b);else discard=true;
    }
  }
  if(uint32_t(micros()-lastRow)>=kRowUs){lastRow=micros();refresh();}
  if(uint32_t(millis()-lastStatus)>=5000){lastStatus=millis();status();}
  delay(1);
}
