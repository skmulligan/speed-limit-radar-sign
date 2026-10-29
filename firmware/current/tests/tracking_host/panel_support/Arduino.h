#pragma once
#include "../runtime_support/Arduino.h"
#define DRAM_ATTR
#define IRAM_ATTR
#define OUTPUT 2
#define portENTER_CRITICAL_ISR(p) portENTER_CRITICAL(p)
#define portEXIT_CRITICAL_ISR(p) portEXIT_CRITICAL(p)
inline void digitalWrite(unsigned,bool){}
