#pragma once
// Deterministic host scheduling/queue adapter, not an ESP32 timing simulation.
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <cstdarg>
using portMUX_TYPE=std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(p) (p)->lock()
#define portEXIT_CRITICAL(p) (p)->unlock()
#define pdTRUE 1
#define pdMS_TO_TICKS(n) (n)
using StackType_t=uint32_t;
struct StaticTask_t {};
using TaskHandle_t=void*;
inline uint64_t fakeUs=100000;
inline uint64_t esp_timer_get_time(){return fakeUs;}
inline void delay(unsigned n){fakeUs+=n*1000;}
inline void vTaskDelay(unsigned){}
inline void vTaskDelete(void*){}
inline void taskYIELD(){}
inline void xTaskNotifyGive(TaskHandle_t){}
inline void ulTaskNotifyTake(int,unsigned){}
inline unsigned uxTaskGetStackHighWaterMark(void*){return 4096;}
inline TaskHandle_t xTaskCreateStaticPinnedToCore(void(*)(void*),const char*,size_t,void*,unsigned,StackType_t*,StaticTask_t* t,unsigned){return t;}
inline unsigned esp_random(){static unsigned n=10;return ++n;}
#define INPUT 0
#define INPUT_PULLUP 1
#define SERIAL_8N1 0
inline void pinMode(unsigned,unsigned){}
struct StaticQueue_t {
  std::mutex mutex;size_t capacity=0,size=0;
  std::deque<std::vector<uint8_t>> items;
};
using QueueHandle_t=StaticQueue_t*;
inline QueueHandle_t xQueueCreateStatic(size_t n,size_t size,uint8_t*,StaticQueue_t* q){q->capacity=n;q->size=size;q->items.clear();return q;}
inline int xQueueSend(QueueHandle_t q,const void* item,unsigned){
  std::lock_guard<std::mutex> guard(q->mutex);
  if(q->items.size()==q->capacity)return 0;
  const auto p=static_cast<const uint8_t*>(item);q->items.emplace_back(p,p+q->size);return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q,void* item,unsigned){
  std::lock_guard<std::mutex> guard(q->mutex);
  if(q->items.empty())return 0;
  memcpy(item,q->items.front().data(),q->size);q->items.pop_front();return pdTRUE;
}
inline unsigned uxQueueMessagesWaiting(QueueHandle_t q){std::lock_guard<std::mutex> guard(q->mutex);return unsigned(q->items.size());}
struct SerialStub {
  std::deque<char> input;std::string output;
  void begin(int){}
  int available(){return int(input.size());}
  int read(){auto c=input.front();input.pop_front();return c;}
  void println(const char* s){output+=s;output+='\n';}
  void printf(const char* f,...){char b[2048];va_list a;va_start(a,f);vsnprintf(b,sizeof(b),f,a);va_end(a);output+=b;}
};
inline SerialStub Serial;
enum hardwareSerial_error_t {UART_ERROR_TEST};
class HardwareSerial {
 public:
  explicit HardwareSerial(int){}
  std::deque<uint8_t> input;
  void setRxBufferSize(int){}
  void onReceiveError(std::function<void(hardwareSerial_error_t)> f){error=f;}
  void onReceive(std::function<void()> f){receive=f;}
  void begin(int,int,int,int){}
  int available(){return int(input.size());}
  int read(){if(input.empty())return -1;auto b=input.front();input.pop_front();return b;}
  void flush(bool){input.clear();}
  std::function<void(hardwareSerial_error_t)> error;
  std::function<void()> receive;
};
