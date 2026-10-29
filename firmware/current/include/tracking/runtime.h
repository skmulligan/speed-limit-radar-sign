#pragma once
#include "records.h"
#include "../radar_reports.h"

namespace tracking { namespace runtime {
constexpr uint64_t kServiceBudgetMs=20;
constexpr unsigned kRecordSlots=64;

struct Anchor {
  Anchor()=default;
  Anchor(uint64_t m,int64_t u,uint16_t q,bool h):mono(m),utc(u),quality(q),rtcHealthy(h){}
  uint64_t mono=0;
  int64_t utc=0;
  uint16_t quality=Unknown;
  bool rtcHealthy=false;
  void stamp(Record& r,uint64_t boot)const {
    r.boot=boot;r.flags=quality;
    r.utc=quality?utc+(int64_t(r.mono)-int64_t(mono)):0;
  }
};

// Single radar owner; copy through a short critical section to consumers.
struct Live {
  radar_reports::Report report{};
  uint64_t sample=0,sequence=0,startupEnd=0,transition=0;
  Anchor anchor{};
  const char* state="starting";
  int selected=-1;
  bool armed=true,valid=false,warning=false;
  unsigned errors=0,invalid=0,backlogs=0,stalls=0;
  uint64_t maxServiceGapUs=0,maxPublishUs=0,reports=0;
  unsigned publishUnder20Ms=0,publishUnder50Ms=0,stackFree=0;
  bool fresh(uint64_t now)const {
    return armed&&valid&&now>=startupEnd&&now>=sample&&now-sample<750;
  }
};

enum class Operation:uint8_t { Record,Sync,Next,Ack,Time,Stop,Live,Initialize };
struct Work {
  Operation op=Operation::Record;
  Record record{};
  Anchor anchor{};
  uint64_t argument=0,cursor=0,enqueued=0;
  uint32_t connection=0,ticket=0;
  bool console=false;
};
struct Reply {
  uint32_t connection=0,ticket=0;
  bool console=false;
  const char* response="status";
  size_t length=0;
  uint8_t bytes[24+16*kRecordSize]{};
};
struct StorageStatus {
  uint64_t device=0,oldest=1,high=0,ack=0,transferHigh=0;
  uint64_t maxAppendUs=0,maxFlushUs=0,maxQueueAgeMs=0,maxCommitAgeMs=0;
  size_t used=0,capacity=0;
  unsigned queueHigh=0,stackFree=0,durabilityOverruns=0;
  bool ready=false,full=false,fault=false;
};

// The barrier is the Work's FIFO position. This class owns transfer identity,
// independently of transport fragmentation, and is also used by host tests.
class Transfer {
 public:
  void begin(uint32_t connection,uint64_t id,uint64_t cursor,uint64_t high) {
    connection_=connection;id_=id;offered_=cursor;high_=high;active_=true;
  }
  bool matches(uint32_t connection,uint64_t id,uint64_t cursor)const {
    return active_&&connection==connection_&&id==id_&&cursor==offered_&&cursor<=high_;
  }
  bool canAck(uint32_t connection,uint64_t cursor)const {
    return active_&&connection==connection_&&cursor<=offered_&&cursor<=high_;
  }
  void offered(uint64_t seq){offered_=seq;}
  uint64_t high()const{return high_;}
 private:
  uint32_t connection_=0;uint64_t id_=0,offered_=0,high_=0;bool active_=false;
};

// Worker-owned recovery. Accepted records preceding a queue overflow are drained
// before Gap. An append failure discards the rest until Gap is durable. v1 Gap
// already marks open phone episodes incomplete, so no invented End is needed.
class Recovery {
 public:
  void failed(){discard_=true;}
  bool discarding()const{return discard_;}
  template<class Store> bool recover(Store& store,Record gap) {
    if(!gapWritten_){if(!store.append(gap))return false;gapWritten_=true;}
    store.flush();
    if(store.fault())return false;
    gapWritten_=false;discard_=false;return true;
  }
 private:bool discard_=false,gapWritten_=false;
};
} }
