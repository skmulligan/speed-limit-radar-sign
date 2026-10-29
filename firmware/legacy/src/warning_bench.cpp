// Attended warning behavior trial; explicitly selected synthetic demo or live radar.
// BATTERY_BENCH opts into automatic live start; CONTINUOUS_RADAR removes the
// live deadline. Neither mode transmits to or configures the radar.
#include <Arduino.h>
#include <atomic>
#include "../include/radar_reports.h"
#include "../include/speed_policy.h"
#include "../include/speed_digits.h"
#include "../include/warning_policy.h"
#include "../include/warning_text.h"
#include "../include/warning_demo.h"
namespace {
constexpr uint8_t kData=11,kClock=12,kLatch=10,kEnable=9,kRowA=4,kRowB=5;
constexpr uint32_t kTrialMs=20000,kRowUs=2000;
#ifdef DAYLIGHT_RADAR_BENCH
#ifndef BATTERY_BENCH
#error "Daylight radar test requires battery startup mode"
#endif
constexpr uint32_t kPulseUs=200;
#else
constexpr uint32_t kPulseUs=50;
#endif
#ifdef CONTINUOUS_RADAR
#ifndef DAYLIGHT_STARTUP_CHECK
#error "Continuous radar requires the startup display marker"
#endif
constexpr bool kContinuousLive=true;
#else
constexpr bool kContinuousLive=false;
#endif
HardwareSerial radar(1);
std::atomic<unsigned> uartErrors{0};
unsigned checkedErrors=0,invalidBytes=0,reportFrames=0,backlogs=0;
speed_policy::State speed;
warning_policy::State warning;
bool demo=false;uint32_t lastDemo=0;
warning_policy::View currentView=warning_policy::View::Blank;
warning_demo::Kind demoKind=warning_demo::Kind::End;
bool armed=false; uint32_t started=0,lastRow=0,lastStatus=0;
#ifdef DAYLIGHT_STARTUP_CHECK
#ifndef DAYLIGHT_RADAR_BENCH
#error "Startup display check requires daylight radar mode"
#endif
constexpr uint32_t kStartupCheckMs=3000;
bool startupCheck=false;
uint32_t startupCheckStarted=0;
#else
constexpr bool startupCheck=false;
#endif
#ifdef BATTERY_BENCH
#ifdef CONTINUOUS_RADAR
constexpr uint32_t kAutoStartDelayMs=0;
#elif defined(DAYLIGHT_RADAR_BENCH)
constexpr uint32_t kAutoStartDelayMs=15000;
#else
constexpr uint32_t kAutoStartDelayMs=5000;
#endif
bool autoStartPending=true;
uint32_t autoStartBase=0;
#endif
uint8_t row=0;
char command[24]; size_t commandSize=0; bool discard=false;
void report(const radar_reports::Report& r,uint32_t now) {
  ++reportFrames; if(demo)return;
  if(!speed.fresh(now))warning.reset();
  speed.update(r,now);
  if(!speed.fresh(now))warning.reset();
  if(speed.fresh(now)) Serial.printf("SAMPLE targets=%u kmh=%u mph=%u direction=1\n",r.count,speed.kmh(),speed.mph());
  else Serial.printf("NO_TARGET eligible=0 targets=%u\n",r.count);
}
void invalid() { ++invalidBytes; if(demo)return; speed.clear();warning.reset();digitalWrite(kEnable,LOW); }
radar_reports::Parser parser(report,invalid);
const char* viewName() {
  if(currentView==warning_policy::View::Message)return "SLOW_DOWN";
  if(currentView==warning_policy::View::Number)return "NUMBER";
  return "BLANK";
}
void status() {
  Serial.printf("WARNING_STATUS armed=%u source=%s fresh=%u mph=%d warning=%u view=%s frames=%u invalid=%u errors=%u backlogs=%u startup_check=%u\n",
    armed,demo ? "SYNTHETIC" : "RADAR",speed.fresh(millis()),speed.fresh(millis()) ? int(speed.mph()) : -1,
    warning.warning(),viewName(),reportFrames,invalidBytes,uartErrors.load(),backlogs,startupCheck);
}
void clearSample() {speed.clear();warning.reset();currentView=warning_policy::View::Blank;digitalWrite(kEnable,LOW);}
void cancelAutoStart() {
#ifdef BATTERY_BENCH
  autoStartPending=false;
#endif
}
void stop() {
  cancelAutoStart();
#ifdef DAYLIGHT_STARTUP_CHECK
  startupCheck=false;
#endif
  armed=false;demo=false;demoKind=warning_demo::Kind::End;clearSample();parser.reset();
}
void startTrial(bool synthetic) {
  cancelAutoStart();
  // Never reuse a sample or partial packet from before explicit/delayed arming.
  for(unsigned n=0;n<4096 && radar.available();++n) radar.read();
  parser.reset();clearSample();started=millis();lastDemo=started;
  demo=synthetic;demoKind=warning_demo::Kind::End;armed=true;
  Serial.printf("WARNING_STARTED source=%s duration_ms=%s limit_mph=20 strict_above=1 expiry_ms=750\n",demo ? "SYNTHETIC" : "RADAR",
    kContinuousLive && !demo ? "unlimited" : "20000");
}
bool withinLiveDeadline() {return (kContinuousLive && !demo) || uint32_t(millis()-started)<kTrialMs;}
void simulate(uint32_t now) {
  auto step=warning_demo::at(uint32_t(now-started));
  if(step.kind!=demoKind) {
    demoKind=step.kind;
    Serial.printf("DEMO_STAGE synthetic=1 kind=%u kmh=%u\n",unsigned(step.kind),step.kmh);
  }
  if(step.kind==warning_demo::Kind::Title){clearSample();return;}
  if(step.kind==warning_demo::Kind::Silence || step.kind==warning_demo::Kind::End)return;
  if(uint32_t(now-lastDemo)<100)return;
  lastDemo=now;
  radar_reports::Report r;
  if(step.kind==warning_demo::Kind::Speed) {r.count=1;r.alarm=1;r.targets[0]={128,2,1,step.kmh,100};}
  if(!speed.fresh(now))warning.reset();
  speed.update(r,now);
  if(!speed.fresh(now))warning.reset();
}
void execute() {
  command[commandSize]=0;
  if(discard) Serial.println("ERROR invalid_command");
  else if(!strcmp(command,"status")) status();
  else if(!strcmp(command,"stop")) { stop();Serial.println("WARNING_STOPPED"); }
  else if(!strcmp(command,"live") || !strcmp(command,"demo-warning")) {
    if(armed || startupCheck) Serial.println("ERROR trial_already_running");
    else startTrial(!strcmp(command,"demo-warning"));
  } else if(commandSize) Serial.println("ERROR unknown_command");
  commandSize=0;discard=false;
}
void refresh() {
  digitalWrite(kEnable,LOW);
  digitalWrite(kRowA,row&1);digitalWrite(kRowB,(row>>1)&1);
  if(!armed)warning.reset();
  const auto view=armed ? warning.view(speed,millis()) : warning_policy::View::Blank;
  if(view!=currentView) {currentView=view;Serial.printf("VIEW source=%s view=%s\n",demo ? "SYNTHETIC" : "RADAR",viewName());}
  const bool title=startupCheck || (armed && demo && demoKind==warning_demo::Kind::Title);
  const bool show=startupCheck || (armed && (title || (speed.fresh(millis()) && view!=warning_policy::View::Blank)));
  const uint8_t mph=speed.mph();
  for(unsigned bit=0;bit<128;++bit) {
    const auto p=panel_mapping::fromSerialBit(row,uint8_t(bit));
    const bool content=startupCheck && kContinuousLive ? warning_text::starting(p) : title ? warning_text::test(p) :
      view==warning_policy::View::Message ? warning_text::slowDown(p) : speed_digits::pixel(mph,p);
    bool lit=show && (content || (demo && !title && warning_text::testMarker(p)));
    digitalWrite(kData,!lit);delayMicroseconds(1);
    digitalWrite(kClock,HIGH);delayMicroseconds(1);digitalWrite(kClock,LOW);
  }
  digitalWrite(kLatch,HIGH);delayMicroseconds(2);digitalWrite(kLatch,LOW);
  // Recheck deadlines after shifting; stale data must not get a new pulse.
  bool withinDeadline=withinLiveDeadline();
#ifdef DAYLIGHT_STARTUP_CHECK
  if(startupCheck)withinDeadline=uint32_t(millis()-startupCheckStarted)<kStartupCheckMs;
#endif
  if(show && (title || speed.fresh(millis())) && withinDeadline && (startupCheck || demo || uartErrors.load()==checkedErrors)) {
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
#ifdef BATTERY_BENCH
  autoStartBase=millis();
#ifdef DAYLIGHT_RADAR_BENCH
#ifdef DAYLIGHT_STARTUP_CHECK
#ifdef CONTINUOUS_RADAR
  Serial.println("CONTINUOUS_RADAR v1 startup_ms=3000 live_duration_ms=unlimited pulse_us=200 rx=17 tx=disabled");
#else
  Serial.println("DAYLIGHT_RADAR_DIAGNOSTIC v1 display_check_delay_ms=15000 display_check_ms=3000 live_duration_ms=20000 pulse_us=200 rx=17 tx=disabled");
#endif
#else
  Serial.println("DAYLIGHT_RADAR_BENCH v1 auto_live_delay_ms=15000 duration_ms=20000 pulse_us=200 no_auto_demo=1 rx=17 tx=disabled limit_mph=20");
#endif
#else
  Serial.println("BATTERY_BENCH v1 auto_live_delay_ms=5000 duration_ms=20000 no_auto_demo=1 rx=17 tx=disabled limit_mph=20");
#endif
#else
  Serial.println("WARNING_BENCH v1 no_auto_display=1 no_auto_demo=1 rx=17 tx=disabled limit_mph=20");
#endif
  status();
}
void loop() {
#ifdef BATTERY_BENCH
#ifdef CONTINUOUS_RADAR
  if(autoStartPending) {
#else
  if(autoStartPending && uint32_t(millis()-autoStartBase)>=kAutoStartDelayMs) {
#endif
#ifdef DAYLIGHT_STARTUP_CHECK
    cancelAutoStart();clearSample();parser.reset();
    startupCheckStarted=millis();startupCheck=true;
    Serial.printf("DISPLAY_CHECK_STARTED pattern=%s synthetic=1 duration_ms=3000 pulse_us=200\n",kContinuousLive ? "STARTING" : "TEST");
#else
    startTrial(false);
#endif
  }
#ifdef DAYLIGHT_STARTUP_CHECK
  if(startupCheck && uint32_t(millis()-startupCheckStarted)>=kStartupCheckMs) {
    startupCheck=false;digitalWrite(kEnable,LOW);
    Serial.println("DISPLAY_CHECK_ENDED");
    startTrial(false);
  }
#endif
#endif
  if(armed && !withinLiveDeadline()) {stop();Serial.println("WARNING_ENDED");}
  // Latch expiry so an old valid flag cannot revive a sample after millis wraps
  // during continuous operation. Fresh UART reports below can repopulate it.
  if(!demo && !speed.fresh(millis())) {speed.clear();warning.reset();}
  const unsigned observed=uartErrors.load();
  if(observed!=checkedErrors || radar.available()>256) {
    checkedErrors=observed;++backlogs;parser.reset();if(!demo)clearSample();
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
  if(armed && demo)simulate(millis());
  if(uint32_t(micros()-lastRow)>=kRowUs){lastRow=micros();refresh();}
  if(uint32_t(millis()-lastStatus)>=5000){lastStatus=millis();status();}
  delay(1);
}
