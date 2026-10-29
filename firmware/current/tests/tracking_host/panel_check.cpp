// Tests the real ISR/publication implementation using simulated GPIO registers.
#include "../../include/tracking/panel.h"
#include <cassert>
using namespace tracking;
int main(){
  assert(panel::begin());
  auto pixels=[](panel_mapping::Pixel){return true;};
  panel::publish(pixels,1000,panel::faults());
  panel::scan(nullptr);assert(enableRises==1&&!(outputBits&(1u<<9)));
  fakeUs=1000000;panel::scan(nullptr);assert(enableRises==1); // expired
  fakeUs=100000;
  auto epoch=panel::faults();unsigned calls=0;
  panel::publish([&](panel_mapping::Pixel){if(++calls==200)panel::fault();return true;},1000,epoch);
  assert(panel::deadline==0);panel::scan(nullptr);assert(enableRises==1);
  panel::publish(pixels,1000,panel::faults());
  bool invalidated=false;
  delayHook=[&]{if(!invalidated){invalidated=true;panel::fault();}};
  panel::scan(nullptr);delayHook=nullptr;
  assert(invalidated&&enableRises==1); // fault after row copied, before ENABLE
  panel::publish(pixels,1000,panel::faults());
  bool changed=false;
  delayHook=[&]{if(!changed){changed=true;panel::publish(pixels,1100,panel::faults());}};
  panel::scan(nullptr);delayHook=nullptr;assert(enableRises==1);
  panel::scan(nullptr);assert(enableRises==2);
}
