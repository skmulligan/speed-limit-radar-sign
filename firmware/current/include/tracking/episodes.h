#pragma once
#include "records.h"
#include "../speed_policy.h"
namespace tracking {
// Sink returns false when persistence cannot accept a record. No unbounded buffer.
class Episodes {
 public:
  using Sink=bool(*)(Record&,void*);
  Episodes(Sink sink,void* context):sink_(sink),context_(context){}
  void sample(const radar_reports::Report& report,uint64_t now){
    if(!tick(now))return;
    speed_policy::State selected;selected.update(report,uint32_t(now));
    if(selected.selectedIndex()<0){close(now,Reason::NoTarget);return;}
    if(!active_){active_=true;start_=now;event_=++nextEvent_;Record r;r.kind=Kind::Start;r.event=event_;r.mono=now;r.values[0]=20;r.values[1]=1;if(!emit(r))return;}
    const uint64_t offset=(now-start_)/1000*1000;
    if(count_&&offset!=offset_&&!flush())return;
    offset_=offset;last_=now;const uint8_t speed=selected.kmh();
    if(!count_){min_=max_=speed;first_=now;}else{if(speed<min_)min_=speed;if(speed>max_)max_=speed;}
    sum_+=speed;++count_;if(report.count>targets_)targets_=report.count;
  }
  bool tick(uint64_t now){
    if(active_&&now-last_>=750)return close(now,Reason::Expired);
    if(active_&&count_&&now>=start_+offset_+1000)return flush();
    return true;
  }
  bool close(uint64_t now,Reason reason){
    if(!active_)return true;
    if(!flush())return false;
    Record r;r.kind=Kind::End;r.event=event_;r.mono=now;r.values[0]=uint32_t(reason);
    r.values[1]=uint32_t(last_);r.values[2]=uint32_t(last_>>32);bool ok=emit(r);active_=false;return ok;
  }
  // A storage discontinuity terminates local aggregation. The worker emits Gap.
  void abandon(){active_=false;count_=sum_=targets_=0;}
  bool active()const{return active_;}
  uint64_t event()const{return active_?event_:0;}
 private:
  bool emit(Record& r){if(sink_(r,context_))return true;abandon();return false;}
  bool flush(){
    if(!count_)return true;
    Record r;r.kind=Kind::Bucket;r.event=event_;r.mono=start_+offset_;
    r.values={uint32_t(last_-first_),min_,max_,sum_,count_,targets_,uint32_t(first_-(start_+offset_))};bool ok=emit(r);
    count_=sum_=targets_=0;return ok;
  }
  Sink sink_;void* context_;bool active_=false;
  uint64_t start_=0,last_=0,first_=0,event_=0,nextEvent_=0,offset_=0;
  uint32_t count_=0,sum_=0,targets_=0,min_=0,max_=0;
};
}
