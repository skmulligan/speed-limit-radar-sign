#pragma once
#include <stdint.h>

// 32x16 / four-phase packing; Freetronics DMD was a scan-order reference.
// Orientation and fixed-20 checks passed on the recorded P10(1R)-4S-V10.
// Other panel variants still require mapping checks.
namespace panel_mapping {
struct Pixel { uint8_t x; uint8_t y; };
constexpr Pixel fromSerialBit(uint8_t phase, uint8_t bit) {
  return {static_cast<uint8_t>(8 * (bit / 32) + bit % 8),
          static_cast<uint8_t>(phase + 4 * (3 - (bit / 8) % 4))};
}
constexpr bool orientationMark(Pixel pixel) {
  return pixel.y == 0 || pixel.x == 0 || (pixel.x == 24 && pixel.y == 12);
}
// Fixed synthetic bench number, never a radar sample. Two 5x7 glyphs at 2x.
inline bool testNumber20(Pixel p) {
  static constexpr uint8_t glyphs[2][7] = {
      {14, 17, 1, 2, 4, 8, 31},
      {14, 17, 17, 17, 17, 17, 14}};
  if (p.y < 1 || p.y > 14 || p.x < 5 || p.x > 26) return false;
  const unsigned x = p.x - 5;
  const unsigned digit = x / 12, column = x % 12;
  return column < 10 && (glyphs[digit][(p.y - 1) / 2] & (1u << (4 - column / 2)));
}
}  // namespace panel_mapping
