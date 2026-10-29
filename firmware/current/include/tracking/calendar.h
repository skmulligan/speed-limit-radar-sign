#pragma once
#include <stdint.h>
namespace tracking {
// Explicit UTC conversion avoids 32-bit time_t on the pinned ESP-IDF 4.4 SDK.
struct Calendar {
  unsigned year,month,day,hour,minute,second;
};
inline bool leap(unsigned year){return year%4==0&&(year%100!=0||year%400==0);}
inline unsigned monthDays(unsigned year,unsigned month){
  const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  return month>=1&&month<=12?days[month-1]+(month==2&&leap(year)):0;
}
inline int64_t toUTC(const Calendar& t){
  if(t.year<2024||t.year>2099||t.month<1||t.month>12||t.day<1||t.day>monthDays(t.year,t.month)||t.hour>23||t.minute>59||t.second>59)return 0;
  int64_t days=0;for(unsigned y=1970;y<t.year;++y)days+=leap(y)?366:365;
  for(unsigned m=1;m<t.month;++m)days+=monthDays(t.year,m);
  days+=t.day-1;return (((days*24+t.hour)*60+t.minute)*60+t.second)*1000;
}
inline bool fromUTC(int64_t utc,Calendar& t){
  if(utc<1704067200000LL||utc>=4102444800000LL)return false;
  uint64_t seconds=uint64_t(utc/1000),days=seconds/86400;
  t.second=seconds%60;t.minute=(seconds/60)%60;t.hour=(seconds/3600)%24;t.year=1970;
  while(days>=unsigned(leap(t.year)?366:365)){days-=leap(t.year)?366:365;++t.year;}
  t.month=1;while(days>=monthDays(t.year,t.month)){days-=monthDays(t.year,t.month);++t.month;}
  t.day=unsigned(days)+1;return true;
}
inline uint8_t bcd(unsigned value){return uint8_t(value/10*16+value%10);}
inline bool decodeRTC(const uint8_t* b,Calendar& t){
  // Reject reserved bits, 12-hour mode and the century bit rather than guessing.
  if((b[0]&0x80)||(b[1]&0x80)||(b[2]&0xc0)||(b[4]&0xc0)||(b[5]&0xe0)||b[3]<1||b[3]>7)return false;
  for(unsigned i=0;i<7;++i)if((b[i]&15)>9||(b[i]>>4)>9)return false;
  auto dec=[](uint8_t v){return unsigned(v>>4)*10+(v&15);};
  t={2000+dec(b[6]),dec(b[5]),dec(b[4]),dec(b[2]),dec(b[1]),dec(b[0])};return toUTC(t)!=0;
}
}
