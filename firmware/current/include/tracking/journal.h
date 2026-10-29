#pragma once
#include <LittleFS.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <unistd.h>
#include "records.h"
namespace tracking {
// Single owner: storage worker. No radar/BLE task may call this class.
class Journal {
 public:
  explicit Journal(const char* mount="/littlefs",int (*syncFile)(int)=::fsync):mount_(mount),syncFile_(syncFile){}
  ~Journal(){if(writer_)fclose(writer_);}
  struct Segment{String path;uint64_t first;size_t slots;};
  bool begin(){
    ready_=LittleFS.begin(false,mount_);if(!ready_)return false;
    Preferences p;p.begin("sd-journal",true);ack_=p.getULong64("ack",0);p.end();
    File root=LittleFS.open("/");for(File f=root.openNextFile();f;f=root.openNextFile()){
      String name=f.name();if(!name.startsWith("/"))name="/"+name;
      if(!name.startsWith("/s-")||!name.endsWith(".bin")||name.length()!=23)continue;
      const uint64_t first=strtoull(name.c_str()+3,nullptr,16);const size_t slots=(f.size()+kRecordSize-1)/kRecordSize;
      if(!first||slots>256||first>uint64_t(INT64_MAX)-slots){fault_=true;continue;}
      if(first&&slots){segments_.push_back({name,first,slots});if(first+slots-1>high_)high_=first+slots-1;}
    }
    std::sort(segments_.begin(),segments_.end(),[](const Segment&a,const Segment&b){return a.first<b.first;});
    for(size_t i=1;i<segments_.size();++i)
      if(segments_[i].first<segments_[i-1].first+segments_[i-1].slots)fault_=true;
    if(ack_>high_)high_=ack_; // All acknowledged segments may have been reclaimed.
    committed_=high_;return true;
  }
  bool initialize(){
    if(ready_)return true;
    if(!LittleFS.format())return false;
    Preferences p;if(!p.begin("sd-journal",false))return false;
    const bool saved=p.putULong64("ack",0)==8;p.end();
    if(!saved)return false;
    return begin();
  }
  void identity(uint64_t boot){boot_=boot;}
  bool append(Record r){
    if(!ready_||fault_)return false;
    if(!writer_||inSegment_>=256){
      seal();if(fault_||!room())return false;
      char path[40];snprintf(path,sizeof(path),"/s-%016llx.bin",(unsigned long long)(high_+1));
      String fullPath=String(mount_)+path;
      writer_=fopen(fullPath.c_str(),"wb");if(!writer_){fault_=true;return false;}
      segments_.push_back({path,high_+1,0});inSegment_=0;
      // Each segment is self-describing even after older segments are reclaimed.
      Record session;session.kind=Kind::Session;session.boot=boot_;session.mono=r.mono;session.utc=r.utc;session.flags=r.flags;
      session.values={kFirmwareRevision,20,750,1000,256,80,0};if(!write(session))return false;
    }
    return write(r);
  }
  void flush(){
    if(writer_&&!fault_){
      if(fflush(writer_)!=0||syncFile_(fileno(writer_))!=0){fault_=true;return;}
      committed_=high_;
    }
  }
  void seal(){flush();if(writer_){fclose(writer_);writer_=nullptr;}inSegment_=0;}
  bool acknowledge(uint64_t sequence){
    if(sequence>committed_)return false;
    if(sequence<=ack_)return true;
    Preferences p;if(!p.begin("sd-journal",false))return false;
    const bool ok=p.putULong64("ack",sequence)==8;p.end();if(ok)ack_=sequence;return ok;
  }
  bool read(uint64_t seq,Record& r){
    if(seq>committed_)return false;
    for(const auto& s:segments_)if(seq>=s.first&&seq<s.first+s.slots){
      File f=LittleFS.open(s.path,"r");uint8_t bytes[kRecordSize]{};
      bool ok=f&&f.seek((seq-s.first)*kRecordSize)&&f.read(bytes,kRecordSize)==kRecordSize&&decode(bytes,kRecordSize,r)&&r.sequence==seq;
      if(!ok){r={};r.kind=Kind::Gap;r.sequence=seq;r.values[0]=2;r.values[1]=1;}return true;
    }
    if(seq>=oldest()&&seq<=committed_){r={};r.kind=Kind::Gap;r.sequence=seq;r.values[0]=2;r.values[1]=1;return true;}
    return false;
  }
  uint64_t oldest()const{return segments_.empty()?committed_+1:segments_.front().first;}
  uint64_t high()const{return committed_;}uint64_t ack()const{return ack_;}
  bool ready()const{return ready_;}bool full()const{return full_;}bool fault()const{return fault_;}
  size_t used()const{return ready_?LittleFS.usedBytes():0;}size_t capacity()const{return ready_?LittleFS.totalBytes():0;}
 private:
  bool room(){
    // Reserve 128 KiB to avoid near-full filesystem pathologies. Reclaim only sealed files.
    while(LittleFS.totalBytes()-LittleFS.usedBytes()<128*1024+256*kRecordSize){
      if(segments_.empty()||segments_.front().first+segments_.front().slots-1>ack_){full_=true;return false;}
      if(!LittleFS.remove(segments_.front().path)){fault_=true;return false;}segments_.erase(segments_.begin());
    }full_=false;return true;
  }
  bool write(Record& r){
    r.sequence=high_+1;auto bytes=encode(r);
    if(fwrite(bytes.data(),1,bytes.size(),writer_)!=bytes.size()){fault_=true;fclose(writer_);writer_=nullptr;return false;}
    ++high_;++inSegment_;++segments_.back().slots;return true;
  }
  bool ready_=false,full_=false,fault_=false;FILE* writer_=nullptr;std::vector<Segment> segments_;
  const char* mount_;
  int (*syncFile_)(int);
  uint64_t high_=0,committed_=0,ack_=0,boot_=0;size_t inSegment_=0;
};
}
