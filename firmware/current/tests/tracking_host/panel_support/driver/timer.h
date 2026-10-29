#pragma once
#include <cstdint>
constexpr int TIMER_COUNT_UP=0,TIMER_PAUSE=0,TIMER_ALARM_EN=1,TIMER_AUTORELOAD_EN=1;
constexpr int TIMER_INTR_LEVEL=0,TIMER_GROUP_0=0,TIMER_0=0,ESP_OK=0;
constexpr int ESP_INTR_FLAG_IRAM=1,ESP_INTR_FLAG_LEVEL3=2;
struct timer_config_t {int divider,counter_dir,counter_en,alarm_en,auto_reload,intr_type;};
inline int timer_init(int,int,timer_config_t*){return 0;}
inline int timer_set_counter_value(int,int,uint64_t){return 0;}
inline int timer_set_alarm_value(int,int,uint64_t){return 0;}
inline int timer_enable_intr(int,int){return 0;}
inline int timer_isr_callback_add(int,int,bool(*)(void*),void*,int){return 0;}
inline int timer_start(int,int){return 0;}
