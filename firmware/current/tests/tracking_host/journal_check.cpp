#include "../../include/tracking/journal.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
using namespace tracking;
int main(){
 std::string name=(std::filesystem::temp_directory_path()/"speed-journal-XXXXXX").string();const char* root=mkdtemp(name.data());assert(root);
 auto reset=[&](){std::filesystem::remove_all(root);std::filesystem::create_directory(root);Preferences::values.clear();Preferences::failWrite=false;LittleFS.capacity=256*1024;LittleFS.mounted=true;LittleFS.failRemove=false;};
 reset();
 {
  Journal j(root);assert(j.begin());j.identity(42);Record r;r.kind=Kind::Start;r.boot=42;
  assert(j.append(r));assert(j.high()==0);j.flush();assert(j.high()==2);
  assert(!j.acknowledge(3));Preferences::failWrite=true;assert(!j.acknowledge(2));assert(j.ack()==0);Preferences::failWrite=false;assert(j.acknowledge(2));assert(j.acknowledge(1));assert(j.ack()==2);j.seal();
 }
 {Journal j(root);assert(j.begin());assert(j.high()==2&&j.ack()==2);Record r;assert(j.read(2,r)&&r.kind==Kind::Start);}
 // Every possible partial final-record length becomes a gap, not invented data.
 for(unsigned cut=1;cut<80;++cut){
  reset();{Journal j(root);assert(j.begin());Record r;r.kind=Kind::Start;assert(j.append(r));j.seal();}
  auto path=std::filesystem::path(root)/"s-0000000000000001.bin";
  {std::ofstream file(path,std::ios::binary|std::ios::app);std::vector<char> zero(cut,0);file.write(zero.data(),zero.size());}
  Journal recovered(root);assert(recovered.begin());assert(recovered.high()==3);Record r;assert(recovered.read(2,r)&&r.kind==Kind::Start);assert(recovered.read(3,r)&&r.kind==Kind::Gap);
  Record next;next.kind=Kind::Clock;assert(recovered.append(next));recovered.flush();assert(recovered.high()==5);
 }
 reset();
 {
  Journal j(root);assert(j.begin());Record r;r.kind=Kind::Bucket;
  // 20,480-byte segment plus 128 KiB reserve. Stop once sealed and capacity is low.
  LittleFS.capacity=128*1024+256*80;
  for(unsigned i=0;i<255;++i)assert(j.append(r));j.flush();assert(j.high()==256);
  assert(!j.append(r));assert(j.full());assert(j.oldest()==1);
  assert(j.acknowledge(256));assert(j.append(r));j.flush();assert(j.oldest()==257&&j.high()==258&&!j.full());
 }
 reset();LittleFS.mounted=false;{Journal j(root);assert(!j.begin());Record r;assert(!j.append(r));assert(!j.ready());}
 reset();
 {
  // Deterministic failure injection; host fsync/device semantics vary by OS.
  Journal j(root,[](int){return -1;});assert(j.begin());
  Record r;assert(j.append(r));j.flush();assert(j.fault());assert(j.high()==0);assert(!j.acknowledge(2));
 }
 std::filesystem::remove_all(root);
}
