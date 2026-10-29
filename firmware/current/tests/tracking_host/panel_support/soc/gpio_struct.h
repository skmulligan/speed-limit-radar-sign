#pragma once
#include <cstdint>
inline uint32_t outputBits=0;
inline unsigned enableRises=0;
struct Register {
  bool set;
  void operator=(uint32_t bits){
    if(set){outputBits|=bits;if(bits&(1u<<9))++enableRises;}
    else outputBits&=~bits;
  }
};
struct GPIOType {Register out_w1ts{true},out_w1tc{false};};
inline GPIOType GPIO;
