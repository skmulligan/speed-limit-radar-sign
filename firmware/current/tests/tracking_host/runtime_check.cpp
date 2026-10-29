// Synthetic execution of the actual task iteration functions with injected I/O.
#define TRACKING_RUNTIME_HOST
#include "../../src/tracking.cpp"
#include <cassert>
#include <thread>

static void frame(uint8_t kmh=16){
  const uint8_t b[]={0xf4,0xf3,0xf2,0xf1,7,0,1,1,128,5,1,kmh,100,0xf8,0xf7,0xf6,0xf5};
  for(auto v:b)radar.input.push_back(v);
}
static void step(){fakeUs+=1000;radarIteration();}
static void advance(unsigned ms){while(ms--)step();}
static void empty(){
  const uint8_t b[]={0xf4,0xf3,0xf2,0xf1,0,0,0xf8,0xf7,0xf6,0xf5};
  for(auto v:b)radar.input.push_back(v);
}
static Work request(Operation op,uint64_t arg=0,uint64_t cursor=0){
  Work w;w.op=op;w.argument=arg;w.cursor=cursor;w.connection=bluetooth::generation.load();w.ticket=++ticket;return w;
}
static void init(){
  assert(work.begin()&&controls.begin()&&replies.begin());bluetooth::begin("test");
  connectionSeen=bluetooth::generation.load();
  current.startupEnd=0;setState("no_target");lastServiceUs=fakeUs;bootID=42;
  liveState.set(current);stored.device=123;storageSnapshot();
}
int main(int argc,char** argv){
  assert(argc==2);const std::string mode=argv[1];init();
  if(mode=="isolation"){
    frame();step();assert(current.selected==0&&panel::deadline>nowMs());
    assert(journal.appends==0&&work.count()==1);
    // A worker paused inside append does not own radar state or its frame lock.
    bool ran=false;journal.duringAppend=[&]{if(!ran){ran=true;frame(22);step();assert(speed.kmh()==22&&panel::deadline>nowMs());}};
    storageIteration();assert(ran&&journal.appends>0);
    const auto before=journal.appends;empty();step();assert(panel::deadline==0&&journal.appends==before);
  }else if(mode=="overflow"){
    for(unsigned i=0;i<30;++i){frame();step();empty();step();}
    assert(loss.get().paused&&loss.get().total>0&&work.count()<=kRecordSlots);
    frame(24);step();assert(speed.kmh()==24&&panel::deadline>nowMs());
    advance(1000);
    while(work.count())storageIteration();storageIteration();
    assert(!loss.get().paused);
    unsigned gaps=0;for(const auto&r:journal.records)gaps+=r.kind==Kind::Gap;assert(gaps==1);
    assert(journal.records.back().kind==Kind::Gap&&journal.high()==journal.records.size());
    const size_t prior=journal.records.size();frame();step();storageIteration();
    assert(journal.records.size()==prior+1&&journal.records.back().kind==Kind::Start);
  }else if(mode=="append-failure"){
    frame();step();empty();step();journal.failAppend=true;storageIteration();
    assert(loss.get().paused&&recovery.discarding());
    frame(20);step();assert(panel::deadline>nowMs());
    advance(1000);storageIteration();assert(loss.get().paused);
    journal.failAppend=false;advance(1000);storageIteration();assert(!loss.get().paused);
    assert(journal.records.size()==1&&journal.records[0].kind==Kind::Gap);
    frame();step();storageIteration();assert(journal.records.back().kind==Kind::Start);
  }else if(mode=="flush-failure"){
    frame();step();storageIteration();journal.failSync=true;
    advance(1000);storageIteration();assert(loss.get().paused&&journal.high()==0);
    advance(1000);storageIteration();assert(loss.get().paused);
    frame(20);step();assert(panel::deadline>nowMs());
    auto time=request(Operation::Time);time.anchor=Anchor(nowMs(),1790000000000,Phone,false);
    handleControl(time);storageIteration();Reply reply;assert(replies.pop(reply));
    assert(!strcmp(reply.response,"time_storage_failed")&&journal.high()==0);
  }else if(mode=="barrier"){
    Record first;first.kind=Kind::Start;first.event=1;first.mono=nowMs();assert(storeRecord(first,nullptr));
    handleControl(request(Operation::Sync,111,0));
    Record second=first;second.event=2;assert(storeRecord(second,nullptr));
    storageIteration();Reply reply;assert(replies.pop(reply)&&reply.length==104);
    assert(get(reply.bytes,8)==1&&journal.records.size()==2&&journal.high()==1);
    auto bad=request(Operation::Ack,2);storageCommand(bad);assert(journal.ack()==0);
    storageCommand(request(Operation::Ack,1));assert(journal.ack()==1);
    auto stale=request(Operation::Next,111,1);++bluetooth::generation;
    storageCommand(stale);assert(!transfer.canAck(bluetooth::generation.load(),1));
    handleControl(request(Operation::Sync,222,1));storageIteration();
    assert(transfer.matches(bluetooth::generation.load(),222,2));
    assert(!transfer.matches(bluetooth::generation.load(),111,2));
  }else if(mode=="clock"){
    anchor=Anchor(0,1790000000000,RTC,true);
    Record first;first.kind=Kind::Bucket;first.mono=50;assert(storeRecord(first,nullptr));
    auto change=request(Operation::Time);change.anchor=Anchor(nowMs(),1800000000000,Phone,false);
    handleControl(change);Record second=first;second.mono=nowMs();assert(storeRecord(second,nullptr));
    ++bluetooth::generation;storageIteration(); // applied TIME survives disconnect
    assert(journal.records.size()==3);
    assert(journal.records[0].utc==1790000000050&&journal.records[0].flags==RTC);
    assert(journal.records[1].kind==Kind::Clock&&journal.records[1].flags==Phone);
    assert(journal.records[2].utc==1800000000000);
    assert(journal.high()==2); // clock acknowledged only after its barrier flush
  }else if(mode=="freshness"){
    frame();step();const auto sequence=current.sequence;
    fakeUs+=100000;frame(25);radarIteration();
    assert(current.sequence==sequence&&!current.valid&&panel::deadline==0&&current.stalls==1);
    frame();step();const auto sample=current.sample;
    assert(liveState.get().fresh(sample+749)&&!liveState.get().fresh(sample+750));
    advance(750);assert(panel::deadline==0);
    radar.input={0xf4,0xf3,0xf2,0xf1,7,0,1,1};step();advance(21);
    assert(!parser.pending());
    frame();step();assert(current.valid);
    auto stop=request(Operation::Stop);stop.console=true;assert(controls.push(stop));step();
    assert(!current.armed&&panel::deadline==0);
    auto restart=request(Operation::Live);restart.console=true;assert(controls.push(restart));frame(25);step();
    assert(!current.valid&&panel::deadline==0);frame();step();assert(current.valid);
  }else if(mode=="uart-race"){
    panel::duringPublish=[](){panel::fault();};frame();step();
    assert(panel::deadline==0);
    live();std::string json(bluetooth::liveMessage.bytes.begin(),bluetooth::liveMessage.bytes.end());
    assert(json.find("\"state\":\"fault\"")!=std::string::npos);
    panel::duringPublish=nullptr;step();assert(!current.valid);
    frame();step();assert(panel::deadline>nowMs());
  }else if(mode=="bucket-boundary"){
    for(unsigned i=0;i<2200;++i){if(i%100==0)frame();step();storageIteration();}
    empty();step();storageIteration();
    std::vector<uint64_t> times;
    for(const auto&r:journal.records)if(r.kind==Kind::Bucket){
      assert(std::find(times.begin(),times.end(),r.mono)==times.end());times.push_back(r.mono);
    }
    assert(times.size()==3);
  }else if(mode=="reply-overflow"){
    Reply stale;stale.ticket=900;
    for(unsigned i=0;i<8;++i)assert(replies.push(stale));
    auto cmd=request(Operation::Live);assert(dispatch(cmd));
    step();storageIteration();assert(replyDropped.load()==pending);
    bleIteration();assert(!pending);
    std::string response(bluetooth::statusMessage.bytes.begin(),bluetooth::statusMessage.bytes.end());
    assert(response.find("response_lost")!=std::string::npos);
  }else if(mode=="stop-pending-storage"){
    frame();step();assert(current.armed&&panel::deadline>nowMs());
    assert(dispatch(request(Operation::Sync,123)));
    step();const auto syncTicket=pending;assert(syncTicket&&controls.count()==0);
    for(char c:std::string("stop\n"))Serial.input.push_back(c);
    bleIteration();step();
    assert(!current.armed&&panel::deadline==0&&pending==syncTicket);
    assert(journal.flushes==0); // Stopping must not wait for the storage task.
    assert(Serial.output.find("BUSY")==std::string::npos);
    bleIteration();assert(Serial.output.find("COMMAND stopped")!=std::string::npos);
    storageIteration();assert(journal.flushes>0);
    bleIteration();assert(!pending); // Original sync completion was preserved.
  }else if(mode=="stop-full-queues"){
    frame();step();
    Work queued;queued.enqueued=nowMs();
    while(work.push(queued)){}
    for(unsigned i=0;i<8;++i){auto oldLive=request(Operation::Live);oldLive.console=true;assert(controls.push(oldLive));}
    Reply oldReply;for(unsigned i=0;i<8;++i)assert(replies.push(oldReply));
    const uint8_t blocked[]={'x'};bluetooth::statusMessage.set(blocked,1);
    Work stop;stop.console=true;stop.op=Operation::Stop;assert(dispatch(stop));
    step();assert(!current.armed&&panel::deadline==0&&stopFlushPending);
    // Neither old live commands nor saturated completion queues undo the stop.
    for(unsigned i=0;i<8;++i){step();assert(!current.armed);}
    bleIteration();assert(Serial.output.find("COMMAND stopped")!=std::string::npos);
    storageIteration();step();assert(!stopFlushPending);
    while(work.count())storageIteration();assert(journal.flushes>0);
  }else if(mode=="stop-rearm"){
    frame();step();
    Work liveCommand;liveCommand.console=true;liveCommand.op=Operation::Live;
    assert(dispatch(liveCommand));const auto liveTicket=pending;
    Work stop;stop.console=true;stop.op=Operation::Stop;
    assert(dispatch(stop));assert(dispatch(stop));assert(pending==liveTicket);
    step();bleIteration();assert(!current.armed&&!pending);
    assert(Serial.output.find("superseded_by_stop")!=std::string::npos);
    assert(dispatch(liveCommand));step();assert(current.armed&&!current.valid);
    frame();step();assert(current.valid&&panel::deadline>nowMs());
  }else if(mode=="time-disconnect"||mode=="time-reconnect"||mode=="time-command"){
    bluetooth::Command cmd;cmd.generation=bluetooth::generation.load();
    strcpy(cmd.text,"TIME 1790000000000");
    assert(xQueueSend(bluetooth::commands,&cmd,0)==pdTRUE);
    clockSource.duringSet=[&]{if(mode!="time-command"){++bluetooth::generation;bluetooth::authorized=(mode=="time-reconnect");}};
    bleIteration();step();
    if(mode=="time-command"){
      assert(anchor.quality==Phone&&pending&&work.count()==1);
      storageIteration();bleIteration();assert(!pending&&journal.high()==1);
      std::string response(bluetooth::statusMessage.bytes.begin(),bluetooth::statusMessage.bytes.end());
      assert(response.find("time_set")!=std::string::npos);
    }else{
      assert(controls.count()==0&&work.count()==0&&!pending&&anchor.quality==Unknown);
      assert(journal.records.empty());
    }
  }else if(mode=="stale-command"){
    auto old=request(Operation::Time);old.anchor=Anchor(nowMs(),1790000000000,Phone,false);
    ++bluetooth::generation;
    clockSource.duringSet=[](){assert(false);};
    command("TIME 1790000000000",old.connection);assert(!dispatch(old));
    assert(!pending&&controls.count()==0&&anchor.quality==Unknown);
    old.connection=bluetooth::generation.load();assert(dispatch(old));
    ++bluetooth::generation;step();bleIteration();
    assert(!pending&&work.count()==0&&anchor.quality==Unknown);
  }else if(mode=="thread-handoffs"){
    Snapshot<Live> snapshot;Loss lost;Queue<unsigned,8> fifo;assert(fifo.begin());
    std::atomic<bool> done{false};
    std::thread producer([&]{for(unsigned n=1;n<=10000;++n){Live l;l.sequence=n;l.sample=n;snapshot.set(l);while(!fifo.push(n))std::this_thread::yield();}done=true;});
    unsigned expect=1;
    while(!done||fifo.count()){
      auto l=snapshot.get();assert(l.sequence==l.sample);
      unsigned n;if(fifo.pop(n)){assert(n==expect);++expect;}
    }
    producer.join();assert(expect==10001);
    std::thread a([&]{for(unsigned i=0;i<1000;++i)lost.mark(i);});
    std::thread b([&]{for(unsigned i=0;i<1000;++i)lost.mark(i);});a.join();b.join();
    assert(lost.get().total==2000&&lost.get().epoch==1);lost.resume();lost.mark(2001);
    assert(lost.get().epoch==2&&lost.get().dropped==1);
  }else assert(false);
}
