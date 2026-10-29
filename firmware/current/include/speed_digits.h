#pragma once
#include "panel_mapping.h"
namespace speed_digits {
// 5x7 numeric font, 2x scale, same geometry as the tested fixed 20.
inline bool pixel(uint8_t mph,panel_mapping::Pixel p) {
  static constexpr uint8_t font[10][7]={
    {14,17,17,17,17,17,14},{4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
  if(mph>99 || p.y<1 || p.y>14) return false;
  const unsigned start=mph<10 ? 11 : 5;
  const unsigned width=mph<10 ? 10 : 22;
  if(p.x<start || p.x>=start+width) return false;
  const unsigned x=p.x-start, column=x%12;
  if(column>=10) return false;
  const unsigned digit=mph<10 ? mph : x<12 ? mph/10 : mph%10;
  return font[digit][(p.y-1)/2] & (1u<<(4-column/2));
}
} // namespace speed_digits
