#pragma once
#include <stdint.h>
namespace warning_demo {
enum class Kind { Title, Speed, Silence, Empty, End };
struct Step {Kind kind;uint8_t kmh;};
inline Step at(uint32_t elapsed) {
  if(elapsed<1000)return {Kind::Title,0};
  if(elapsed<4000)return {Kind::Speed,29}; // rounds to 18 MPH
  if(elapsed<12000)return {Kind::Speed,40}; // rounds to 25 MPH
  if(elapsed<14000)return {Kind::Silence,0}; // expire during warning message
  if(elapsed<17000)return {Kind::Speed,29};
  if(elapsed<20000)return {Kind::Empty,0};
  return {Kind::End,0};
}
} // namespace warning_demo
