// Tracking v2: radar, storage and BLE/control have independent single owners.
// Build/test only does not establish physical timing; see tracking-validation.md.
#ifdef TRACKING_RUNTIME_HOST
#include "runtime_platform.h"
#else
#include <Arduino.h>
#include <atomic>
#include <Preferences.h>
#include <esp_timer.h>

#include "../include/tracking/journal.h"
#include "../include/tracking/clock.h"
#include "../include/tracking/panel.h"
#include "../include/tracking/bluetooth.h"
#include "../include/tracking/task_channels.h"
#endif
#include "../include/tracking/runtime.h"
#include "../include/tracking/episodes.h"
#include "../include/warning_policy.h"
#include "../include/warning_text.h"
#include "../include/speed_digits.h"

using namespace tracking;
using namespace tracking::runtime;
namespace {
uint64_t nowUs(){return uint64_t(esp_timer_get_time());}
uint64_t nowMs(){return nowUs()/1000;}
uint64_t bootID=0;
HardwareSerial radar(1);
Journal journal;                    // storage owner after setup
Clock clockSource;                  // BLE/control owner after setup
Queue<Work,kRecordSlots+8> work;      // radar -> storage; 8 slots reserved for controls
Queue<Work,8> controls;              // BLE/control -> radar
Queue<Reply,8> replies;              // radar/storage -> BLE; one request in flight
Snapshot<Live> liveState;
Snapshot<StorageStatus> storageState;
Loss loss;
std::atomic<unsigned> queueHigh{0};
std::atomic<uint32_t> pauseObserved{0};
std::atomic<uint32_t> replyDropped{0};
// Coalescing console stop mailbox, independent of control/storage/reply capacity.
std::atomic<uint32_t> stopRequested{0},stopApplied{0};
std::atomic<bool> ready{false},fatal{false};
TaskHandle_t radarHandle=nullptr,storageHandle=nullptr,bleHandle=nullptr;
// ESP-IDF's task size argument is bytes, not words. Static arrays are internal RAM.
static StackType_t radarStack[6144/sizeof(StackType_t)];
static StackType_t storageStack[8192/sizeof(StackType_t)];
static StackType_t bleStack[10240/sizeof(StackType_t)];
static StaticTask_t radarTCB,storageTCB,bleTCB;

// Radar-owned state. No worker reads these objects directly.
Live current;
speed_policy::State speed;
warning_policy::State warning;
Anchor anchor;
uint64_t parseStartedUs=0,lastServiceUs=0,partialSince=0,batchSampleMono=0;
uint32_t seenLoss=0;
bool stopFlushPending=false;
void setState(const char* state){
  if(strcmp(current.state,state)){current.state=state;++current.transition;}
}
bool enqueue(Work item){
  item.enqueued=nowMs();
  if(!work.push(item))return false;
  const unsigned n=work.count();if(n>queueHigh.load())queueHigh.store(n);
  if(storageHandle)xTaskNotifyGive(storageHandle);
  return true;
}
bool storeRecord(Record& record,void*){
  anchor.stamp(record,bootID);
  // Only radar produces Work, so admission leaves controls a fixed reserve.
  if(loss.get().paused)return false;
  Work item;item.record=record;
  if(work.count()>=kRecordSlots||!enqueue(item)){loss.mark(nowMs());return false;}
  return true;
}
Episodes episodes(storeRecord,nullptr);
void publish(){
  const uint64_t now=nowMs();
  const auto view=warning.view(speed,uint32_t(now));
  current.warning=warning.warning();current.selected=speed.selectedIndex();
  if(current.armed&&now<current.startupEnd){
    panel::publish([](panel_mapping::Pixel p){return warning_text::starting(p);},current.startupEnd,current.errors);
  }else if(current.armed&&speed.fresh(uint32_t(now))&&view!=warning_policy::View::Blank){
    const uint8_t mph=speed.mph();
    panel::publish([&](panel_mapping::Pixel p){return view==warning_policy::View::Message?
      warning_text::slowDown(p):speed_digits::pixel(mph,p);},current.sample+750,current.errors);
  }else panel::clear();
  // A callback after this check clears independently; before it is caught here.
  if(panel::faults()!=current.errors)panel::clear();
}
void clear(Reason reason,const char* state){
  speed.clear();warning.reset();panel::clear();current.valid=false;
  current.selected=-1;current.warning=false;setState(state);
  episodes.close(nowMs(),reason);
}
void discardInput();
void report(const radar_reports::Report& report,uint32_t){
  const uint64_t now=nowMs();
  if(!current.armed||now<current.startupEnd)return;
  if(nowUs()-lastServiceUs>kServiceBudgetMs*1000||panel::faults()!=current.errors){
    ++current.stalls;clear(Reason::Uart,"fault");discardInput();return;
  }
  // Bound from the preceding service, including partial-frame retention. Using
  // "now - budget" here would hide time spent parsing this batch after it waited.
  const uint64_t sample=batchSampleMono;
  if(!speed.fresh(uint32_t(now)))warning.reset();
  speed.update(report,uint32_t(sample));
  current.report=report;current.sample=sample;++current.sequence;current.valid=true;
  setState(speed.selectedIndex()>=0?"valid":report.count?"no_eligible":"no_target");
  publish();                        // before aggregation/queue operations
  if(panel::faults()!=current.errors){clear(Reason::Uart,"fault");discardInput();return;}
  const uint64_t duration=nowUs()-parseStartedUs;
  ++current.reports;current.maxPublishUs=std::max(current.maxPublishUs,duration);
  if(duration<=20000)++current.publishUnder20Ms;
  if(duration<=50000)++current.publishUnder50Ms;
  if(!loss.get().paused)episodes.sample(report,sample);
}
void invalid(){++current.invalid;clear(Reason::Invalid,"fault");}
radar_reports::Parser parser(report,invalid);
void discardInput(){parser.reset();partialSince=0;radar.flush(false);}
void sendReply(const Work& item,const char* response){
  Reply r;r.connection=item.connection;r.ticket=item.ticket;r.console=item.console;r.response=response;
  if(!replies.push(r)){++bluetooth::commandDrops;replyDropped.store(item.ticket);}
}
void handleControl(Work item){
  if(!item.console&&item.connection!=bluetooth::generation.load())return;
  if(item.op==Operation::Stop){
    current.armed=false;clear(Reason::Stop,"stopped");
  }else if(item.op==Operation::Live){
    const uint32_t stop=stopRequested.load();
    // An older queued live command must not re-arm after the urgent stop.
    if(stop&&(item.ticket==stop||uint32_t(item.ticket-stop)>=0x80000000u)){
      sendReply(item,"superseded_by_stop");return;
    }
    clear(Reason::Stop,"no_target");discardInput();current.armed=true;
  }else if(item.op==Operation::Time){
    Record clock;clock.kind=Kind::Clock;clock.mono=nowMs();
    item.anchor.stamp(clock,bootID);item.record=clock;
    // Clock Work and its flush precede records stamped with this anchor.
    if(!enqueue(item)){sendReply(item,"busy");return;}
    anchor=item.anchor;current.anchor=anchor;return;
  }
  if(!enqueue(item))sendReply(item,"busy");
}
void radarIteration(){
    const uint64_t service=nowUs(),now=service/1000;
    const uint64_t previousMs=lastServiceUs/1000;
    batchSampleMono=previousMs>=kServiceBudgetMs?previousMs-kServiceBudgetMs:0;
    const uint64_t gap=service-lastServiceUs;lastServiceUs=service;
    current.maxServiceGapUs=std::max(current.maxServiceGapUs,gap);
    const auto lost=loss.get();
    if(lost.epoch!=seenLoss){episodes.abandon();seenLoss=lost.epoch;}
    if(lost.paused)pauseObserved.store(lost.epoch);
    if(now>=current.startupEnd&&!strcmp(current.state,"starting")){setState("no_target");discardInput();}
    if(current.valid&&current.report.count&&now-current.sample>=750)clear(Reason::Expired,"expired");
    const unsigned observed=panel::faults();
    if(gap>kServiceBudgetMs*1000||observed!=current.errors||radar.available()>256){
      current.errors=observed;++current.backlogs;if(gap>kServiceBudgetMs*1000)++current.stalls;
      clear(Reason::Uart,"fault");discardInput();
    }
    Work control;if(controls.pop(control))handleControl(control);
    const uint32_t requested=stopRequested.load();
    if(requested!=stopApplied.load()){
      current.armed=false;clear(Reason::Stop,"stopped");
      stopFlushPending=true;stopApplied.store(requested);
    }
    if(stopFlushPending){
      Work marker;marker.op=Operation::Stop;marker.console=true;
      // Ticket zero is an internal flush barrier, not a command completion.
      // Retry without blocking if even the reserved control capacity is full.
      if(enqueue(marker))stopFlushPending=false;
    }
    if(parser.pending()&&now-partialSince>kServiceBudgetMs){
      clear(Reason::Invalid,"fault");discardInput();++current.invalid;
    }
    parseStartedUs=nowUs();
    for(unsigned i=0;i<256&&radar.available();++i){
      if(nowUs()-service>kServiceBudgetMs*1000){clear(Reason::Uart,"fault");discardInput();++current.stalls;break;}
      if(!parser.pending())partialSince=nowMs();
      int byte=radar.read();if(byte>=0)parser.feed(uint8_t(byte),uint32_t(nowMs()));
    }
    publish();
    if(!loss.get().paused)episodes.tick(batchSampleMono);
    current.stackFree=uxTaskGetStackHighWaterMark(nullptr);liveState.set(current);
    // Bounded sleep advances warning phases/expiry even with no UART events.

  }
void radarTask(void*){
  while(!ready.load())ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1));
  if(fatal.load()){vTaskDelete(nullptr);return;}
  current=liveState.get();anchor=current.anchor;
  radar.setRxBufferSize(4096);
  radar.onReceiveError([](hardwareSerial_error_t){panel::fault();if(radarHandle)xTaskNotifyGive(radarHandle);});
  radar.onReceive([](){if(radarHandle)xTaskNotifyGive(radarHandle);});
  radar.begin(115200,SERIAL_8N1,17,-1);pinMode(17,INPUT_PULLUP);pinMode(18,INPUT);
  if(!panel::begin()){fatal.store(true);panel::clear();setState("fault");current.armed=false;liveState.set(current);vTaskDelete(nullptr);return;}
  current.startupEnd=nowMs()+3000;lastServiceUs=nowUs();
  for(;;){radarIteration();ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1));}
}

// Storage owner. No call below occurs in the radar task or BLE callback.
StorageStatus stored;
Transfer transfer;
Recovery recovery;
Anchor storageAnchor;
uint64_t dirtySince=0,lastFlush=0,lastRecovery=0,lastStats=0;
bool dirty=false;
void noteDirty(const Record& r){
  const uint64_t since=r.kind==Kind::Bucket?r.mono+r.values[6]:r.mono;
  if(!dirty||since<dirtySince)dirtySince=since;
  dirty=true;
}
bool append(const Record& r){
  const auto start=nowUs();bool ok=journal.append(r);
  stored.maxAppendUs=std::max(stored.maxAppendUs,nowUs()-start);
  if(ok)noteDirty(r);
  else {loss.mark(nowMs());recovery.failed();}
  return ok;
}
void flush(){
  const auto start=nowUs();journal.flush();
  stored.maxFlushUs=std::max(stored.maxFlushUs,nowUs()-start);lastFlush=nowMs();
  if(dirty){
    const uint64_t age=lastFlush>=dirtySince?lastFlush-dirtySince:0;
    stored.maxCommitAgeMs=std::max(stored.maxCommitAgeMs,age);
    if(age>2000)++stored.durabilityOverruns;
  }
  if(journal.fault()){if(!loss.get().paused)loss.mark(nowMs(),0);recovery.failed();}
  else dirty=false;
}
void storageSnapshot(bool filesystemStats=false){
  stored.ready=journal.ready();stored.full=journal.full();stored.fault=journal.fault();
  stored.oldest=journal.oldest();stored.high=journal.high();stored.ack=journal.ack();
  stored.transferHigh=transfer.high();stored.queueHigh=queueHigh.load();
  stored.stackFree=uxTaskGetStackHighWaterMark(nullptr);
  if(filesystemStats){stored.used=journal.used();stored.capacity=journal.capacity();}
  storageState.set(stored);
}
void history(const Work& item){
  if(item.op==Operation::Sync){
    flush();
    if(journal.fault()){sendReply(item,"storage_fault");return;}
    if(item.cursor>journal.high()){sendReply(item,"bad_cursor");return;}
    transfer.begin(item.connection,item.argument,item.cursor,journal.high());
  }
  if(!transfer.matches(item.connection,item.argument,item.cursor)){sendReply(item,"bad_transfer");return;}
  Reply response;response.connection=item.connection;response.ticket=item.ticket;response.console=item.console;
  put(response.bytes,transfer.high(),8);put(response.bytes+8,journal.oldest(),8);put(response.bytes+16,item.argument,8);
  response.length=24;
  uint64_t next=std::max(item.cursor+1,journal.oldest());
  for(unsigned i=0;i<16&&next<=transfer.high();++i,++next){
    Record r;if(!journal.read(next,r))break;auto bytes=encode(r);
    memcpy(response.bytes+response.length,bytes.data(),bytes.size());response.length+=bytes.size();transfer.offered(next);
    if(dirty&&nowMs()-lastFlush>=1000)flush();
    taskYIELD();
  }
  storageSnapshot();
  if(!replies.push(response)){++bluetooth::commandDrops;replyDropped.store(item.ticket);}
}
void storageCommand(const Work& item){
  // TIME was applied by radar before enqueue; persist its anchor even if the
  // phone disconnects meanwhile. Transport discards the obsolete reply.
  if(!item.console&&item.op!=Operation::Time&&item.connection!=bluetooth::generation.load())return;
  switch(item.op){
    case Operation::Sync:case Operation::Next:history(item);return;
    case Operation::Ack:
      if(transfer.canAck(item.connection,item.argument)&&journal.acknowledge(item.argument)){
        storageSnapshot();sendReply(item,"acknowledged");
      }else sendReply(item,"bad_ack");
      return;
    case Operation::Time:{
      storageAnchor=item.anchor;
      bool ok=append(item.record);flush();storageSnapshot();
      sendReply(item,ok&&!journal.fault()?"time_set":"time_storage_failed");return;
    }
    case Operation::Initialize:
      if(!journal.ready()){
        const uint64_t replacement=(uint64_t(esp_random())<<32)|esp_random();
        Preferences p;p.begin("sd-device",false);bool saved=p.putULong64("id",replacement)==8;p.end();
        if(saved){stored.device=replacement;saved=journal.initialize();}
        storageSnapshot(true);sendReply(item,saved?"storage_ready":"storage_failed");
      }else sendReply(item,"already_mounted");
      return;
    case Operation::Stop:
      flush();storageSnapshot();if(item.ticket)sendReply(item,"stopped");return;
    case Operation::Live:sendReply(item,"live");return;
    case Operation::Record:return;
  }
}
void storageIteration(){
    Work item;
    // A SYNC's FIFO position is a barrier after all earlier accepted records.
    for(unsigned n=0;n<8&&work.pop(item);++n){
      stored.maxQueueAgeMs=std::max(stored.maxQueueAgeMs,nowMs()-item.enqueued);
      if(item.op==Operation::Record){
        if(recovery.discarding())loss.mark(nowMs());else append(item.record);
      }else storageCommand(item);
      if(dirty&&nowMs()-lastFlush>=1000)flush();
    }
    if(dirty&&nowMs()-lastFlush>=1000)flush();
    const auto lost=loss.get();
    // Acknowledgement prevents an old episode enqueue after recovery.
    if(lost.paused&&pauseObserved.load()==lost.epoch&&!work.count()&&nowMs()-lastRecovery>=1000){
      lastRecovery=nowMs();Record gap;gap.kind=Kind::Gap;gap.mono=lastRecovery;
      gap.values={1,lost.dropped,uint32_t(lost.since),uint32_t(lost.since>>32),0,0,0};
      storageAnchor.stamp(gap,bootID);
      const auto start=nowUs();
      if(recovery.recover(journal,gap)){dirty=false;loss.resume();}
      stored.maxFlushUs=std::max(stored.maxFlushUs,nowUs()-start);
    }
    const auto now=nowMs();bool stats=now-lastStats>=2000;if(stats)lastStats=now;
    storageSnapshot(stats);

  }
void storageTask(void*){
  while(!ready.load())ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1));
  if(fatal.load()){vTaskDelete(nullptr);return;}
  stored=storageState.get();storageAnchor=liveState.get().anchor;lastFlush=nowMs();
  for(;;){storageIteration();ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(2));}
}

// BLE/control owner. Formatting, I2C, console and Message objects stay here.
uint32_t ticket=0,pending=0,connectionSeen=0;
uint32_t liveFaults=0;
uint32_t stopReported=0;
bool pendingConsole=false;
uint64_t lastLive=0,lastStatus=0,liveTransition=0,liveDeadline=0;
void status(const char* response="status",uint64_t token=0){
  if(bluetooth::statusMessage.pending())return;
  const auto s=storageState.get();const auto l=liveState.get();const auto lost=loss.get();
  char b[1500];
  snprintf(b,sizeof(b),"{\"v\":1,\"response\":\"%s\",\"token\":\"%llu\",\"device\":\"%016llx\",\"boot\":\"%016llx\",\"mono\":%llu,\"oldest\":\"%llu\",\"high\":\"%llu\",\"ack\":\"%llu\",\"transferHigh\":\"%llu\",\"used\":%u,\"capacity\":%u,\"storage\":\"%s\",\"clockQuality\":%u,\"rtcHealthy\":%s,\"dropped\":%u,\"uartErrors\":%u,\"invalidBytes\":%u,\"backlogs\":%u,\"commandDrops\":%u,\"serviceGapUs\":%llu,\"publishUs\":%llu,\"appendUs\":%llu,\"flushUs\":%llu,\"queueHigh\":%u,\"queueAgeMs\":%llu,\"commitAgeMs\":%llu,\"durabilityOverruns\":%u,\"stalls\":%u}",
    response,(unsigned long long)token,(unsigned long long)s.device,(unsigned long long)bootID,(unsigned long long)nowMs(),
    (unsigned long long)s.oldest,(unsigned long long)s.high,(unsigned long long)s.ack,(unsigned long long)s.transferHigh,
    unsigned(s.used),unsigned(s.capacity),!s.ready?"uninitialized":s.fault?"fault":s.full?"full":lost.paused?"paused":"recording",
    l.anchor.quality,l.anchor.rtcHealthy?"true":"false",lost.total,panel::faults(),l.invalid,l.backlogs,bluetooth::commandDrops.load(),
    (unsigned long long)l.maxServiceGapUs,(unsigned long long)l.maxPublishUs,(unsigned long long)s.maxAppendUs,
    (unsigned long long)s.maxFlushUs,s.queueHigh,(unsigned long long)s.maxQueueAgeMs,(unsigned long long)s.maxCommitAgeMs,s.durabilityOverruns,l.stalls);
  bluetooth::statusMessage.set(reinterpret_cast<uint8_t*>(b),strlen(b));
}
void live(){
  const auto s=liveState.get();const uint64_t now=nowMs();
  const auto faults=panel::faults();
  const bool fresh=s.fresh(now)&&faults==s.errors;
  const bool changed=s.transition!=liveTransition||faults!=liveFaults;
  if(changed||(liveDeadline&&now>=liveDeadline)){
    bluetooth::liveMessage.reset();lastLive=0;liveTransition=s.transition;liveFaults=faults;liveDeadline=0;
  }
  if(bluetooth::liveMessage.pending()||(!changed&&now-lastLive<100))return;
  const char* state=s.state;
  if(s.valid&&!fresh)state=panel::faults()!=s.errors?"fault":"expired";
  char b[1400];int n=snprintf(b,sizeof(b),"{\"v\":1,\"boot\":\"%016llx\",\"sequence\":\"%llu\",\"sampleMono\":%llu,\"sentMono\":%llu,\"state\":\"%s\",\"selected\":%d,\"warning\":%s,\"alarm\":%u,\"targets\":[",
    (unsigned long long)bootID,(unsigned long long)s.sequence,(unsigned long long)s.sample,(unsigned long long)now,
    state,fresh?s.selected:-1,fresh&&s.warning?"true":"false",s.report.alarm);
  if(fresh)for(unsigned i=0;i<s.report.count;++i){const auto t=s.report.targets[i];
    n+=snprintf(b+n,sizeof(b)-size_t(n),"%s[%d,%u,%u,%u,%u]",i?",":"",int(t.angle)-128,t.distance,t.direction,t.speedKmh,t.snr);
  }
  snprintf(b+n,sizeof(b)-size_t(n),"]}");
  bluetooth::liveMessage.set(reinterpret_cast<uint8_t*>(b),strlen(b));lastLive=now;
  liveDeadline=fresh?s.sample+750:0;
}
bool dispatch(Work item){
  if(!item.console&&(item.connection!=bluetooth::generation.load()||!bluetooth::authorized.load()))return false;
  if(fatal.load()){if(item.console)Serial.println("FAULT radar unavailable");else status("radar_fault");return false;}
  if(item.console&&item.op==Operation::Stop){
    if(++ticket==0)++ticket;
    stopRequested.store(ticket);xTaskNotifyGive(radarHandle);return true;
  }
  if(pending){if(item.console)Serial.println("BUSY");else status("busy");return false;}
  if(item.op==Operation::Live&&stopRequested.load()!=stopApplied.load()){
    if(item.console)Serial.println("BUSY stop pending");else status("busy");return false;
  }
  if(++ticket==0)++ticket;
  item.ticket=ticket; // Preserve the ingress connection, including across RTC I/O.
  if(!controls.push(item)){++bluetooth::commandDrops;if(item.console)Serial.println("BUSY");else status("busy");return false;}
  pending=item.ticket;pendingConsole=item.console;
  xTaskNotifyGive(radarHandle);return true;
}
void command(const char* text,uint32_t ingressConnection){
  if(ingressConnection!=bluetooth::generation.load()||!bluetooth::authorized.load())return;
  char verb[20]{};unsigned long long arg=0,cursor=0;char extra;
  int fields=sscanf(text,"%19s %llu %llu %c",verb,&arg,&cursor,&extra);
  if(fields<1||fields>3){status("bad_command");return;}
  if(fields==1&&!strcmp(verb,"STATUS")){status();return;}
  Work item;item.connection=ingressConnection;item.argument=arg;item.cursor=cursor;
  if(fields==3&&(!strcmp(verb,"SYNC")||!strcmp(verb,"NEXT"))){
    if(bluetooth::historyMessage.pending()&&!strcmp(verb,"NEXT")){status("busy");return;}
    item.op=!strcmp(verb,"SYNC")?Operation::Sync:Operation::Next;
    if(item.op==Operation::Sync)bluetooth::historyMessage.reset();
    dispatch(item);return;
  }
  if(fields!=2){status("bad_command");return;}
  if(!strcmp(verb,"PING")){status("pong",arg);return;}
  if(!strcmp(verb,"ACK")){item.op=Operation::Ack;dispatch(item);return;}
  if(!strcmp(verb,"TIME")){
    const uint64_t at=nowMs();
    const bool set=clockSource.set(int64_t(arg),at);
    // The I2C transaction may span a disconnect or reconnect. It cannot be
    // undone, but stale work must not update radar state or reply to a new peer.
    if(ingressConnection!=bluetooth::generation.load()||!bluetooth::authorized.load())return;
    if(!set){status("bad_time");return;}
    item.op=Operation::Time;item.anchor={at,int64_t(arg),Phone,clockSource.healthy()};dispatch(item);return;
  }
  status("bad_command");
}
void consoleStatus(){
  const auto s=storageState.get();const auto l=liveState.get();const auto lost=loss.get();
  Serial.printf("TRACKING v2 ready=%u full=%u fault=%u paused=%u clock=%u rtc=%u high=%llu dropped=%u uart=%u backlogs=%u stalls=%u\n",
    s.ready,s.full,s.fault,lost.paused,l.anchor.quality,l.anchor.rtcHealthy,(unsigned long long)s.high,lost.total,panel::faults(),l.backlogs,l.stalls);
  Serial.printf("TIMING service_gap_us=%llu parse_publish_us=%llu reports=%llu within20ms=%u within50ms=%u append_us=%llu flush_us=%llu queue_high=%u queue_age_ms=%llu commit_age_ms=%llu overruns=%u stack_free=%u/%u/%u\n",
    (unsigned long long)l.maxServiceGapUs,(unsigned long long)l.maxPublishUs,(unsigned long long)l.reports,l.publishUnder20Ms,l.publishUnder50Ms,
    (unsigned long long)s.maxAppendUs,(unsigned long long)s.maxFlushUs,s.queueHigh,(unsigned long long)s.maxQueueAgeMs,
    (unsigned long long)s.maxCommitAgeMs,s.durabilityOverruns,l.stackFree,s.stackFree,unsigned(uxTaskGetStackHighWaterMark(nullptr)));
}
void console(){
  static char b[40];static unsigned used=0;static bool overflow=false;
  for(unsigned i=0;i<32&&Serial.available();++i){int c=Serial.read();
    if(c=='\n'||c=='\r'){
      b[used]=0;
      if(!overflow&&used){
        Work item;item.console=true;
        if(!strcmp(b,"status"))consoleStatus();
        else if(!strcmp(b,"pair"))bluetooth::pair(nowMs());
        else if(!strcmp(b,"forget-phone"))bluetooth::forget();
        else if(!strcmp(b,"init-storage")){item.op=Operation::Initialize;dispatch(item);}
        else if(!strcmp(b,"stop")){item.op=Operation::Stop;dispatch(item);}
        else if(!strcmp(b,"live")){item.op=Operation::Live;dispatch(item);}
      }
      used=0;overflow=false;
    }else if(c>=32&&c<=126){if(used<sizeof(b)-1)b[used++]=char(c);else overflow=true;}else overflow=true;
  }
}
void bleIteration(){
    const uint32_t generation=bluetooth::generation.load();
    if(generation!=connectionSeen){
      connectionSeen=generation;
      bluetooth::statusMessage.reset();bluetooth::historyMessage.reset();bluetooth::liveMessage.reset();
      lastLive=0;liveDeadline=0;if(!pendingConsole)pending=0;
    }
    const uint32_t stopped=stopApplied.load();
    if(stopped!=stopReported){
      stopReported=stopped;Serial.println("COMMAND stopped");
    }
    if(pending&&replyDropped.load()==pending&&!bluetooth::statusMessage.pending()){
      pending=0;
      if(pendingConsole)Serial.println("COMMAND response_lost; inspect status");
      else status("response_lost");
    }
    Reply reply;
    if(!bluetooth::statusMessage.pending()&&replies.pop(reply)){
      if(reply.ticket==pending&&(reply.console||reply.connection==generation)){
        pending=0;
        if(reply.console)Serial.printf("COMMAND %s\n",reply.response);
        else if(reply.length)bluetooth::historyMessage.set(reply.bytes,reply.length);
        else status(reply.response);
      }
    }
    console();
    bluetooth::Command cmd;
    if(!pending&&!bluetooth::statusMessage.pending()&&xQueueReceive(bluetooth::commands,&cmd,0)==pdTRUE){
      if(cmd.generation==generation&&bluetooth::authorized.load())command(cmd.text,cmd.generation);
    }
    const auto now=nowMs();
    if(bluetooth::authorized.load()){
      live();
      if(!pending&&now-lastStatus>=2000){status();lastStatus=now;}
    }
    // Formatting/status work or a flash stall may have consumed the remaining
    // lifetime since live() copied the snapshot. Recheck before fragment send.
    if(liveDeadline&&(nowMs()>=liveDeadline||panel::faults()!=liveFaults)){
      bluetooth::liveMessage.reset();liveDeadline=0;lastLive=0;
    }
    bluetooth::tick(nowMs());
  }
void bleTask(void*){
  while(!ready.load())ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1));
  if(fatal.load()){vTaskDelete(nullptr);return;}
  for(;;){bleIteration();vTaskDelay(pdMS_TO_TICKS(1));}
}
} // namespace

void setup(){
  Serial.begin(115200);delay(500);
  bootID=(uint64_t(esp_random())<<32)|esp_random();
  Preferences p;p.begin("sd-device",false);stored.device=p.getULong64("id",0);
  if(!stored.device){stored.device=(uint64_t(esp_random())<<32)|esp_random();p.putULong64("id",stored.device);}p.end();
  clockSource.begin(nowMs());
  const uint64_t at=nowMs();current.anchor={at,clockSource.utc(at),clockSource.quality(),clockSource.healthy()};
  liveState.set(current);
  journal.begin();journal.identity(bootID);
  Record start;start.kind=Kind::Session;start.mono=nowMs();start.values={kFirmwareRevision,20,750,1000,256,80,0};
  current.anchor.stamp(start,bootID);append(start);flush();storageSnapshot(true);
  bluetooth::begin("Street Speed");
  if(!work.begin()||!controls.begin()||!replies.begin())fatal.store(true);
  if(!fatal.load()){
    radarHandle=xTaskCreateStaticPinnedToCore(radarTask,"radar",sizeof(radarStack),nullptr,4,radarStack,&radarTCB,1);
    storageHandle=xTaskCreateStaticPinnedToCore(storageTask,"storage",sizeof(storageStack),nullptr,1,storageStack,&storageTCB,0);
    bleHandle=xTaskCreateStaticPinnedToCore(bleTask,"ble-control",sizeof(bleStack),nullptr,2,bleStack,&bleTCB,0);
    if(!radarHandle||!storageHandle||!bleHandle)fatal.store(true);
  }
  ready.store(true);
  Serial.println(fatal.load()?"FATAL task initialization":"TRACKING v2 EXPERIMENTAL tasks=radar/storage/ble; status, pair, forget-phone, init-storage, stop, live");
}
void loop(){vTaskDelay(pdMS_TO_TICKS(1000));}
