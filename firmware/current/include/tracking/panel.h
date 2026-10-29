#pragma once
#include <Arduino.h>
#include <driver/timer.h>
#include <soc/gpio_struct.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include "../panel_mapping.h"
namespace tracking {
// Timer and frame storage use internal RAM, never PSRAM. ISR code calls ROM only.
namespace panel {
static DRAM_ATTR uint32_t frames[4][4]{};
static DRAM_ATTR uint64_t deadline=0;
static DRAM_ATTR uint8_t row=0;
static DRAM_ATTR uint32_t faultEpoch=0;
static DRAM_ATTR portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
inline void IRAM_ATTR pin(unsigned p,bool v){if(v)GPIO.out_w1ts=1u<<p;else GPIO.out_w1tc=1u<<p;}
static bool IRAM_ATTR scan(void*){
  pin(9,false);uint32_t bits[4];uint64_t expires;
  portENTER_CRITICAL_ISR(&lock);for(unsigned i=0;i<4;++i)bits[i]=frames[row][i];expires=deadline;portEXIT_CRITICAL_ISR(&lock);
  pin(4,row&1);pin(5,(row>>1)&1);
  for(unsigned i=0;i<128;++i){pin(11,!(bits[i/32]&(1u<<(i%32))));esp_rom_delay_us(1);pin(12,true);esp_rom_delay_us(1);pin(12,false);}
  pin(10,true);esp_rom_delay_us(2);pin(10,false);
  // Recheck under the same lock as invalidation. A fault between row copy and
  // ENABLE must not relight the cached row after clear().
  portENTER_CRITICAL_ISR(&lock);
  const bool show=deadline==expires&&uint64_t(esp_timer_get_time()/1000)<deadline;
  if(show)pin(9,true);
  portEXIT_CRITICAL_ISR(&lock);
  if(show){esp_rom_delay_us(200);pin(9,false);}
  row=(row+1)&3;return false;
}
inline void clear(){portENTER_CRITICAL(&lock);deadline=0;portEXIT_CRITICAL(&lock);GPIO.out_w1tc=1u<<9;}
inline void fault(){portENTER_CRITICAL(&lock);++faultEpoch;deadline=0;portEXIT_CRITICAL(&lock);GPIO.out_w1tc=1u<<9;}
inline uint32_t faults(){portENTER_CRITICAL(&lock);auto n=faultEpoch;portEXIT_CRITICAL(&lock);return n;}
template<typename Pixel> void publish(Pixel pixel,uint64_t expires,uint32_t expectedFaults){
  uint32_t next[4][4]{};for(unsigned r=0;r<4;++r)for(unsigned i=0;i<128;++i)if(pixel(panel_mapping::fromSerialBit(r,uint8_t(i))))next[r][i/32]|=1u<<(i%32);
  portENTER_CRITICAL(&lock);
  if(faultEpoch==expectedFaults){for(unsigned r=0;r<4;++r)for(unsigned i=0;i<4;++i)frames[r][i]=next[r][i];deadline=expires;}
  portEXIT_CRITICAL(&lock);
}
inline bool begin(){
  for(uint8_t p:{9,11,12,10,4,5}){digitalWrite(p,p==11);pinMode(p,OUTPUT);}
  timer_config_t c{};c.divider=80;c.counter_dir=TIMER_COUNT_UP;c.counter_en=TIMER_PAUSE;c.alarm_en=TIMER_ALARM_EN;c.auto_reload=TIMER_AUTORELOAD_EN;c.intr_type=TIMER_INTR_LEVEL;
  return timer_init(TIMER_GROUP_0,TIMER_0,&c)==ESP_OK&&timer_set_counter_value(TIMER_GROUP_0,TIMER_0,0)==ESP_OK&&timer_set_alarm_value(TIMER_GROUP_0,TIMER_0,2000)==ESP_OK&&timer_enable_intr(TIMER_GROUP_0,TIMER_0)==ESP_OK&&timer_isr_callback_add(TIMER_GROUP_0,TIMER_0,scan,nullptr,ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_LEVEL3)==ESP_OK&&timer_start(TIMER_GROUP_0,TIMER_0)==ESP_OK;
}
}
}
