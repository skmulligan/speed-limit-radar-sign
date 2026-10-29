#pragma once
#include <Wire.h>
#include "records.h"
#include "calendar.h"
namespace tracking {
class Clock {
 public:
  void begin(uint64_t now){
    Wire.begin(6,7,100000);Wire.setTimeOut(20);
    int64_t value=readUTC();if(!value)return;
    anchor_=now;utc_=value;quality_=RTC;rtcHealthy_=true;
  }
  bool set(int64_t utc,uint64_t now){
    Calendar t;if(!fromUTC(utc,t))return false;
    // Day-of-week is informational and does not affect the date registers.
    uint8_t b[]={0,bcd(t.second),bcd(t.minute),bcd(t.hour),uint8_t((utc/86400000+4)%7+1),bcd(t.day),bcd(t.month),bcd(t.year-2000)};
    Wire.beginTransmission(0x68);Wire.write(b,sizeof(b));bool ok=Wire.endTransmission()==0;
    uint8_t control=0,status=0;
    // Enable the oscillator in battery mode before clearing the oscillator-stop flag.
    if(ok&&read(0x0e,&control,1)&&read(0x0f,&status,1)){
      Wire.beginTransmission(0x68);Wire.write(0x0e);Wire.write(control&~0x80);Wire.write(status&~0x80);ok=Wire.endTransmission()==0;
    }else ok=false;
    const int64_t check=ok?readUTC():0;
    ok=check>=utc-999&&check<=utc+1000;
    anchor_=now;utc_=utc;quality_=Phone;rtcHealthy_=ok;return true;
  }
  int64_t utc(uint64_t now)const{return quality_?utc_+int64_t(now)-int64_t(anchor_):0;}
  uint16_t quality()const{return quality_;}bool healthy()const{return rtcHealthy_;}
 private:
  int64_t readUTC(){
    uint8_t b[7],control=0,status=0;Calendar t;
    if(!read(0x0e,&control,1)||!read(0x0f,&status,1)||(control&0x80)||(status&0x80)||!read(0,b,7)||!decodeRTC(b,t))return 0;
    return toUTC(t);
  }
  bool read(uint8_t reg,uint8_t* b,size_t n){
    Wire.beginTransmission(0x68);Wire.write(reg);if(Wire.endTransmission(false))return false;
    if(Wire.requestFrom(uint8_t(0x68),uint8_t(n))!=n)return false;
    for(size_t i=0;i<n;++i)b[i]=Wire.read();
    return true;
  }
  uint64_t anchor_=0;int64_t utc_=0;uint16_t quality_=Unknown;bool rtcHealthy_=false;
};
}
