#include "../../include/radar_query.h"
#include <cassert>
#include <vector>
#include <string>
#include <iostream>
using Bytes=std::vector<uint8_t>;
std::vector<Bytes> sent,acks; std::vector<std::string> events;
void send(const uint8_t* p,size_t n){sent.emplace_back(p,p+n);}
void event(const char* s){events.emplace_back(s);}
void ack(const uint8_t* p,size_t n){acks.emplace_back(p,p+n);}
using radar_query::Engine;
Bytes response(unsigned cmd,unsigned length,unsigned status=0) {
  Bytes p={0xfd,0xfc,0xfb,0xfa,uint8_t(length),0,uint8_t(cmd),1,uint8_t(status),0};
  p.resize(6+length);p.insert(p.end(),{4,3,2,1});return p;
}
void feed(Engine& e,const Bytes& b,uint32_t now){for(auto v:b)e.feed(v,now);}
bool has(const char* s){for(auto& v:events)if(v==s)return true;return false;}
void clear(){sent.clear();acks.clear();events.clear();}
int main(){
  Engine e(send,event,ack);
  e.tick(5000);assert(sent.empty());
  assert(e.start(100));
  assert(sent.back()==Bytes({0xfd,0xfc,0xfb,0xfa,4,0,0xff,0,1,0,4,3,2,1}));
  assert(!e.start(200));assert(sent.size()==1);
  feed(e,{0xf4,0xf3,0xf2,0xf1,0,0,0xf8,0xf7,0xf6,0xf5},210);
  feed(e,response(0x12,8),220);assert(sent.size()==1); // unrelated ACK
  const unsigned cmds[]={0xff,0x12,0x13,0xa0,0xfe};
  const unsigned lengths[]={8,8,8,12,4};
  for(unsigned i=0;i<5;++i){
    if(i)assert(sent.back()==Bytes({0xfd,0xfc,0xfb,0xfa,2,0,uint8_t(cmds[i]),0,4,3,2,1}));
    feed(e,response(cmds[i],lengths[i]),300+i*100);
  }
  assert(!e.active()&&sent.size()==5&&has("QUERY_COMPLETE")&&has("CONFIG_EXIT_CONFIRMED"));
  // Enter timeout: attempt end, then bounded failure if end also times out.
  clear();e.start(0);e.tick(1999);assert(sent.size()==1);
  e.tick(2000);assert(sent.size()==2&&sent.back()[6]==0xfe);
  e.tick(4000);assert(!e.active()&&has("QUERY_FAILED")&&has("CONFIG_EXIT_UNCONFIRMED"));
  // Failed status never advances to a read; successful cleanup is still failure.
  clear();e.start(0);feed(e,response(0xff,4,1),10);
  assert(sent.size()==2&&sent.back()[6]==0xfe);
  feed(e,response(0xfe,4),20);assert(!e.active()&&has("QUERY_FAILED"));
  // Malformed/oversized envelope cannot satisfy the pending ACK.
  clear();e.start(0);feed(e,{0xfd,0xfc,0xfb,0xfa,255,255},10);
  Bytes bad=response(0xff,8);bad.back()=0;feed(e,bad,20);assert(sent.size()==1);
  feed(e,response(0xff,8),30);assert(sent.back()[6]==0x12);
  e.cancel(40);assert(sent.back()[6]==0xfe);feed(e,response(0xfe,4),50);
  // Short firmware ACK is not silently accepted despite the vendor typo.
  clear();e.start(0);feed(e,response(0xff,8),1);feed(e,response(0x12,8),2);
  feed(e,response(0x13,8),3);feed(e,response(0xa0,11),4);
  assert(sent.back()[6]==0xfe&&has("ERROR ack_shape"));
  feed(e,response(0xfe,4),5);assert(has("QUERY_FAILED"));
  // A byte-count mismatch also fails closed and allows eventual resync.
  clear();e.start(0);feed(e,response(0xff,8),1);feed(e,response(0x12,8),2);
  feed(e,response(0x13,8),3);bad=response(0xa0,12);bad[4]=11;feed(e,bad,4);
  e.tick(2003);assert(sent.back()[6]==0xfe);feed(e,response(0xfe,4),2004);
  assert(!e.active()&&has("QUERY_FAILED"));
  // Deadline subtraction across millis wrap, with no retry extension.
  clear();e.start(0xfffffff0u);e.tick(uint32_t(0xfffffff0u+2000u));
  assert(sent.back()[6]==0xfe);feed(e,response(0xfe,4),2100);
  assert(!e.active());
  std::cout<<"PASS synthetic query engine: exact read-only packets, ACK matching, malformed input, bounded cleanup, cancellation, rollover\n";
}
