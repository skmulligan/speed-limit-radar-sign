#include "../../include/tracking/calendar.h"
#include <cassert>
using namespace tracking;
int main(){
  Calendar t;assert(fromUTC(1704067200000LL,t)&&t.year==2024&&t.month==1&&t.day==1);
  assert(toUTC({2024,2,30,0,0,0})==0);assert(toUTC({2025,2,29,0,0,0})==0);
  assert(toUTC({2024,2,29,0,0,0})==1709164800000LL);
  assert(toUTC({2038,1,19,3,14,8})==2147483648000LL);
  assert(toUTC({2099,12,31,23,59,59})==4102444799000LL);
  // Round-trip every supported day, including the 2038 boundary and leap days.
  for(int64_t utc=1704067200000LL;utc<4102444800000LL;utc+=86400000){assert(fromUTC(utc,t));assert(toUTC(t)==utc);}
  uint8_t raw[]={0,0,0,2,0x29,0x02,0x24};assert(decodeRTC(raw,t));raw[4]=0x30;assert(!decodeRTC(raw,t));raw[4]=0x29;raw[2]=0x40;assert(!decodeRTC(raw,t));
  assert(!fromUTC(4102444800000LL,t));
}
