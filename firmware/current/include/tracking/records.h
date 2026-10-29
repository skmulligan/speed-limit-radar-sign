#pragma once
#include <stdint.h>
#include <stddef.h>
#include <array>
namespace tracking {
constexpr size_t kRecordSize=80;
constexpr uint32_t kMagic=0x31474453; // SDG1, little endian
constexpr uint8_t kVersion=1;
constexpr uint32_t kFirmwareRevision=2;
enum class Kind:uint8_t {Session=1,Start=2,Bucket=3,End=4,Clock=5,Gap=6};
enum class Reason:uint32_t {NoTarget=1,Invalid=2,Uart=3,Expired=4,Stop=5,Reboot=6,Storage=7};
enum TimeQuality:uint16_t {Unknown=0,RTC=1,Phone=2};
struct Record {
  Kind kind=Kind::Session; uint16_t flags=0;
  uint64_t sequence=0,boot=0,mono=0,event=0; int64_t utc=0;
  std::array<uint32_t,7> values{};
};
inline uint32_t crc32(const uint8_t* data,size_t length) {
  uint32_t c=~0u;for(size_t i=0;i<length;++i){c^=data[i];for(int b=0;b<8;++b)c=(c>>1)^(0xedb88320u & (0u-(c&1)));}return ~c;
}
inline void put(uint8_t* p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)p[i]=uint8_t(v>>(8*i));}
inline uint64_t get(const uint8_t* p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(p[i])<<(8*i);return v;}
inline std::array<uint8_t,kRecordSize> encode(const Record& r){
  std::array<uint8_t,kRecordSize> b{};put(b.data(),kMagic,4);b[4]=kVersion;b[5]=uint8_t(r.kind);put(&b[6],r.flags,2);
  put(&b[8],r.sequence,8);put(&b[16],r.boot,8);put(&b[24],r.mono,8);put(&b[32],r.event,8);put(&b[40],uint64_t(r.utc),8);
  for(unsigned i=0;i<7;++i)put(&b[48+4*i],r.values[i],4);
  put(&b[76],crc32(b.data(),76),4);return b;
}
inline bool decode(const uint8_t* b,size_t n,Record& r){
  if(n!=kRecordSize||get(b,4)!=kMagic||b[4]!=kVersion||b[5]<1||b[5]>6||get(b+76,4)!=crc32(b,76))return false;
  r.kind=Kind(b[5]);r.flags=uint16_t(get(b+6,2));r.sequence=get(b+8,8);r.boot=get(b+16,8);r.mono=get(b+24,8);r.event=get(b+32,8);r.utc=int64_t(get(b+40,8));
  for(unsigned i=0;i<7;++i)r.values[i]=uint32_t(get(b+48+4*i,4));
  return true;
}
// Transport fragments: message ID, total bytes including CRC, offset (u16 LE).
// Accept default ATT values and larger negotiated values; one assembly per characteristic.
class Assembly {
 public:
  bool feed(const uint8_t* p,size_t n){
    if(n<7||n>244){reset();return false;}
    unsigned id=unsigned(get(p,2)),total=unsigned(get(p+2,2)),off=unsigned(get(p+4,2));
    if(off==0){used_=0;id_=id;total_=total;}
    if(id!=id_||total!=total_||total<4||total>sizeof(data_)||off!=used_||used_+n-6>total){reset();return false;}
    for(size_t i=6;i<n;++i)data_[used_++]=p[i];
    if(used_!=total_)return false;
    if(get(data_+used_-4,4)!=crc32(data_,used_-4)){reset();return false;}return true;
  }
  const uint8_t* data()const{return data_;} size_t size()const{return used_>=4?used_-4:0;}
  void reset(){used_=0;total_=0;}
 private:uint8_t data_[2048]{};unsigned used_=0,total_=0,id_=0;
};
}
