#include "../../include/tracking/episodes.h"
#include <cassert>
#include <vector>
#include <fstream>
#include <string>
using namespace tracking;
static bool sink(Record& r,void* p){auto& v=*static_cast<std::vector<Record>*>(p);r.sequence=v.size()+1;r.boot=42;v.push_back(r);return true;}
static radar_reports::Report target(uint8_t speed=40){radar_reports::Report r;r.count=1;r.alarm=1;r.targets[0]={128,5,1,speed,100};return r;}
int main(int argc,char** argv){
  std::vector<Record> records;Episodes e(sink,&records);
  e.sample(target(30),100);e.sample(target(40),600);e.sample(target(50),1099);e.sample(target(60),1100);e.close(1200,Reason::NoTarget);
  assert(records.size()==4&&records[1].kind==Kind::Bucket);
  assert(records[1].values[1]==30&&records[1].values[2]==50&&records[1].values[3]==120&&records[1].values[4]==3);
  assert(records[2].mono==1100&&records[2].values[4]==1&&records[3].values[1]==1100);
  e.sample(target(),2000);e.tick(2749);assert(e.active());e.tick(2750);assert(!e.active());
  auto multiple=target();multiple.count=2;multiple.targets[1]={130,9,1,70,90};
  speed_policy::State policy;policy.update(multiple,0);assert(policy.selectedIndex()==1);
  multiple.targets[1].speedKmh=40;policy.update(multiple,1);assert(policy.selectedIndex()==0);
  multiple.targets[0].direction=0;multiple.targets[1].direction=0;policy.update(multiple,2);assert(policy.selectedIndex()==-1);
  e.sample(target(),uint64_t(UINT32_MAX)-10);e.sample(target(),uint64_t(UINT32_MAX)+50);assert(e.active());e.tick(uint64_t(UINT32_MAX)+800);assert(!e.active());
  for(auto r:records){r.utc=1790000000000;r.flags=RTC;auto bytes=encode(r);Record d;assert(decode(bytes.data(),bytes.size(),d)&&d.utc==r.utc&&d.sequence==r.sequence);bytes[60]^=1;assert(!decode(bytes.data(),bytes.size(),d));}
  Assembly assembly;uint8_t b[20]{};put(b,1,2);put(b+2,7,2);b[6]='a';b[7]='b';b[8]='c';put(b+9,crc32(b+6,3),4);assert(assembly.feed(b,13)&&assembly.size()==3);b[4]=1;assert(!assembly.feed(b,13));
  uint8_t large[210]{};put(large,2,2);put(large+2,204,2);
  for(unsigned i=0;i<200;++i)large[6+i]=uint8_t(i);
  put(large+206,crc32(large+6,200),4);
  assert(assembly.feed(large,sizeof(large))&&assembly.size()==200);
  // Export a deterministic synthetic golden wire fixture for Swift interop tests.
  if(argc==2){std::ofstream out(argv[1],std::ios::binary);for(size_t i=0;i<4;++i){auto r=records[i];r.utc=1790000000000+int64_t(r.mono);r.flags=RTC;auto b=encode(r);out.write(reinterpret_cast<const char*>(b.data()),b.size());}}
}
