#pragma once
#include "panel_mapping.h"
namespace warning_text {
inline uint8_t glyph(char letter,unsigned row) {
  static constexpr uint8_t letters[9][7]={
    {15,16,16,14,1,1,30}, // S
    {16,16,16,16,16,16,31}, // L
    {14,17,17,17,17,17,14}, // O
    {17,17,17,21,21,21,10}, // W
    {30,17,17,17,17,17,30}, // D
    {17,25,25,21,19,19,17}, // N
    {31,4,4,4,4,4,4}, // T
    {31,16,16,30,16,16,31}, // E
    {0,0,0,0,0,0,0}};
  const char names[]="SLOWDNTE";
  for(unsigned i=0;i<8;++i)if(names[i]==letter)return letters[i][row];
  return 0;
}
inline bool word(const char* text,panel_mapping::Pixel p,unsigned top) {
  if(p.x<4 || p.x>26 || p.y<top || p.y>=top+7)return false;
  unsigned x=p.x-4,column=x%6;
  return column<5 && (glyph(text[x/6],p.y-top)&(1u<<(4-column)));
}
inline bool slowDown(panel_mapping::Pixel p) {return word("SLOW",p,0)||word("DOWN",p,8);}
inline bool test(panel_mapping::Pixel p) {return word("TEST",p,4);}
// Eight 3x5 glyphs plus seven one-pixel gaps fit a single 31-pixel line.
inline bool starting(panel_mapping::Pixel p) {
  static constexpr uint8_t letters[8][5]={
    {7,4,7,1,7}, // S
    {7,2,2,2,2}, // T
    {2,5,7,5,5}, // A
    {6,5,6,5,5}, // R
    {7,2,2,2,2}, // T
    {7,2,2,2,7}, // I
    {5,7,7,7,5}, // N
    {7,4,5,5,7}  // G
  };
  if(p.x>=31 || p.y<5 || p.y>=10 || p.x%4==3)return false;
  return letters[p.x/4][p.y-5] & (1u<<(2-p.x%4));
}
// Persistent small T at top-left distinguishes synthetic number/message screens.
inline bool testMarker(panel_mapping::Pixel p) {
  return (p.y==0 && p.x<3)||(p.x==1 && p.y<3);
}
} // namespace warning_text
