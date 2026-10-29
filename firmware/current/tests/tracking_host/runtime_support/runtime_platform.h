#pragma once
#include <Arduino.h>
#include "../support/Preferences.h"
#include "../../../include/tracking/records.h"
#include "../../../include/tracking/task_channels.h"
#include "../../../include/panel_mapping.h"
namespace tracking {
class Journal {
 public:
  bool mounted=true,failed=false,fullFlag=false,failAppend=false,failSync=false;
  unsigned appends=0,flushes=0;uint64_t committed=0,acked=0;
  std::vector<Record> records;
  std::function<void()> duringAppend,duringFlush;
  bool begin(){return mounted;}
  bool initialize(){mounted=true;return true;}
  void identity(uint64_t){}
  bool append(Record r){
    ++appends;if(duringAppend)duringAppend();
    if(!mounted||failed||failAppend)return false;
    r.sequence=records.size()+1;records.push_back(r);return true;
  }
  void flush(){++flushes;if(duringFlush)duringFlush();if(failSync)failed=true;if(!failed)committed=records.size();}
  bool fault()const{return failed;}
  bool full()const{return fullFlag;}
  bool ready()const{return mounted;}
  uint64_t oldest()const{return 1;}
  uint64_t high()const{return committed;}
  uint64_t ack()const{return acked;}
  size_t used()const{return records.size()*80;}
  size_t capacity()const{return 1024*1024;}
  bool acknowledge(uint64_t n){if(n>committed)return false;acked=std::max(acked,n);return true;}
  bool read(uint64_t n,Record& r){if(!n||n>committed)return false;r=records[n-1];return true;}
};
class Clock {
 public:
  std::function<void()> duringSet;
  void begin(uint64_t){}
  int64_t utc(uint64_t)const{return 0;}
  uint16_t quality()const{return Unknown;}
  bool healthy()const{return false;}
  bool set(int64_t u,uint64_t){if(duringSet)duringSet();return u>0;}
};
namespace panel {
inline uint32_t epoch=0;
inline uint64_t deadline=0;
inline unsigned published=0;
inline std::function<void()> duringPublish;
inline bool begin(){return true;}
inline void clear(){deadline=0;}
inline void fault(){++epoch;deadline=0;}
inline uint32_t faults(){return epoch;}
template<class Pixel> void publish(Pixel pixel,uint64_t expires,uint32_t expected){
  (void)pixel(panel_mapping::fromSerialBit(0,0));
  if(duringPublish)duringPublish();
  if(epoch==expected){deadline=expires;++published;}
}
}
namespace bluetooth {
inline std::atomic<uint32_t> generation{1};
inline std::atomic<bool> authorized{true};
inline std::atomic<unsigned> commandDrops{0};
struct Command {char text[128]{};uint32_t generation=0;};
inline StaticQueue_t commandControl;
inline uint8_t commandBytes[8*sizeof(Command)];
inline QueueHandle_t commands=nullptr;
struct Message {
  std::vector<uint8_t> bytes;
  void set(const uint8_t* p,size_t n){bytes.assign(p,p+n);}
  bool pending()const{return !bytes.empty();}
  void reset(){bytes.clear();}
};
inline Message statusMessage,historyMessage,liveMessage;
inline void begin(const char*){commands=xQueueCreateStatic(8,sizeof(Command),commandBytes,&commandControl);}
inline void pair(uint64_t){}
inline void forget(){++generation;}
inline void tick(uint64_t){}
}
}
