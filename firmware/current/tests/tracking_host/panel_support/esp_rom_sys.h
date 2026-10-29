#pragma once
#include <Arduino.h>
inline std::function<void()> delayHook;
inline void esp_rom_delay_us(unsigned us){fakeUs+=us;if(delayHook)delayHook();}
