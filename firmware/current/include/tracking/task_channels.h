#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace tracking { namespace runtime {
// Instances are static globals in internal DRAM. No allocation after begin().
template<class T,unsigned N> class Queue {
 public:
  bool begin(){handle_=xQueueCreateStatic(N,sizeof(T),bytes_,&control_);return handle_!=nullptr;}
  bool push(const T& item){return xQueueSend(handle_,&item,0)==pdTRUE;}
  bool pop(T& item){return xQueueReceive(handle_,&item,0)==pdTRUE;}
  unsigned count()const{return uxQueueMessagesWaiting(handle_);}
 private:
  StaticQueue_t control_{};alignas(4) uint8_t bytes_[N*sizeof(T)]{};
  QueueHandle_t handle_=nullptr;
};
template<class T> class Snapshot {
 public:
  T get(){portENTER_CRITICAL(&lock_);T copy=value_;portEXIT_CRITICAL(&lock_);return copy;}
  void set(const T& value){portENTER_CRITICAL(&lock_);value_=value;portEXIT_CRITICAL(&lock_);}
 private:portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;T value_{};
};

struct LossState {bool paused=false;uint32_t epoch=0;uint64_t since=0;unsigned dropped=0,total=0;};
class Loss {
 public:
  LossState get(){portENTER_CRITICAL(&lock_);auto s=state_;portEXIT_CRITICAL(&lock_);return s;}
  void mark(uint64_t now,unsigned count=1){
    portENTER_CRITICAL(&lock_);
    if(!state_.paused){state_.paused=true;state_.since=now;++state_.epoch;}
    state_.dropped+=count;state_.total+=count;
    portEXIT_CRITICAL(&lock_);
  }
  // Called only after producer has observed pause and all prior Work is drained.
  void resume(){portENTER_CRITICAL(&lock_);state_.paused=false;state_.dropped=0;portEXIT_CRITICAL(&lock_);}
 private:portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;LossState state_{};
};
} }
