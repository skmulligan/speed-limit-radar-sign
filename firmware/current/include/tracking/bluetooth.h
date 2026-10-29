#pragma once
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>
#include <atomic>
#include "records.h"
namespace tracking {
namespace bluetooth {
constexpr char serviceUUID[]="73640001-7065-6564-6465-746563746f72";
constexpr char statusUUID[]="73640002-7065-6564-6465-746563746f72";
constexpr char controlUUID[]="73640003-7065-6564-6465-746563746f72";
constexpr char liveUUID[]="73640004-7065-6564-6465-746563746f72";
constexpr char historyUUID[]="73640005-7065-6564-6465-746563746f72";
static BLEServer* server=nullptr;
static BLECharacteristic *statusChar=nullptr,*controlChar=nullptr,*liveChar=nullptr,*historyChar=nullptr;
static std::atomic<bool> connected{false},authorized{false},disconnected{false};
static std::atomic<uint64_t> pairingUntil{0};
static std::atomic<uint64_t> connectedAt{0};
static QueueHandle_t commands=nullptr;
static std::atomic<unsigned> commandDrops{0};
static std::atomic<uint32_t> generation{0};
static portMUX_TYPE ownerLock=portMUX_INITIALIZER_UNLOCKED;
static uint8_t owner[6]{};
static std::atomic<bool> hasOwner{false};
struct Command{char text[128];uint32_t generation=0;};
static StaticQueue_t commandControl;
static uint8_t commandStorage[8*sizeof(Command)];
class Security:public BLESecurityCallbacks {
  uint32_t onPassKeyRequest()override{return 0;}
  void onPassKeyNotify(uint32_t pin)override{Serial.printf("PAIR passkey=%06u\n",pin);}
  bool onConfirmPIN(uint32_t)override{return false;}
  bool onSecurityRequest()override{return pairingUntil.load()>uint64_t(esp_timer_get_time()/1000)||esp_ble_get_bond_device_num()>0;}
  void onAuthenticationComplete(esp_ble_auth_cmpl_t result)override{
    // Compare with the owner loaded BEFORE pairing, not the bond list which may
    // already contain a newly bonded, unauthorized peer by this callback.
    portENTER_CRITICAL(&ownerLock);
    const bool known=hasOwner&&!memcmp(owner,result.bd_addr,6);
    const bool enrolling=!hasOwner&&pairingUntil.load()>uint64_t(esp_timer_get_time()/1000);
    const bool ok=result.success&&(result.auth_mode&ESP_LE_AUTH_REQ_MITM)&&(known||enrolling);
    if(ok){memcpy(owner,result.bd_addr,6);hasOwner=true;}
    portEXIT_CRITICAL(&ownerLock);
    authorized.store(ok);
    if(!ok){if(!known)esp_ble_remove_bond_device(result.bd_addr);server->disconnect(server->getConnId());}
    else {pairingUntil.store(0);}
  }
};
class ServerCallbacks:public BLEServerCallbacks {
 void onConnect(BLEServer* s)override{if(connected.exchange(true)){s->disconnect(s->getConnId());return;}++generation;connectedAt.store(uint64_t(esp_timer_get_time()/1000));authorized.store(false);}
 void onDisconnect(BLEServer*)override{++generation;connected.store(false);authorized.store(false);disconnected.store(true);}
};
class ControlCallbacks:public BLECharacteristicCallbacks {
 Assembly assembly;
 uint32_t assemblyGeneration=0;
 void onWrite(BLECharacteristic* c)override{
   if(!authorized.load())return;
   const uint32_t current=generation.load();
   if(current!=assemblyGeneration){assembly.reset();assemblyGeneration=current;}
   auto bytes=c->getValue();
   if(assembly.feed(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size())){
     if(assembly.size()<sizeof(Command::text)){
       Command cmd{};cmd.generation=current;memcpy(cmd.text,assembly.data(),assembly.size());
       if(xQueueSend(commands,&cmd,0)!=pdTRUE)++commandDrops;
     }assembly.reset();
   }
 }
};
// In pinned Arduino 2.0.17, notify invokes onStatus synchronously with the
// esp_ble_gatts_send_indicate result. Never advance a fragment rejected by GATT.
static std::atomic<bool> notificationAccepted{false};
class DeliveryCallbacks:public BLECharacteristicCallbacks {
 void onStatus(BLECharacteristic*,Status result,uint32_t)override{
   notificationAccepted.store(result==Status::SUCCESS_NOTIFY);
 }
};
struct Message {
  uint8_t bytes[2048]{};size_t size=0,offset=0;uint16_t id=0;
  void set(const uint8_t* p,size_t n){if(n>sizeof(bytes)-4)return;memcpy(bytes,p,n);put(bytes+n,crc32(p,n),4);size=n+4;offset=0;++id;}
  bool pending()const{return offset<size;}
  void reset(){size=offset=0;}
  void send(BLECharacteristic* c){
    if(!pending())return;
    uint8_t b[244];const size_t mtu=std::max(size_t(23),size_t(server->getPeerMTU(server->getConnId())));
    const size_t n=std::min(size-offset,std::min(size_t(244),mtu-3)-6);
    put(b,id,2);put(b+2,size,2);put(b+4,offset,2);memcpy(b+6,bytes+offset,n);
    c->setValue(b,n+6);notificationAccepted.store(false);c->notify();
    if(notificationAccepted.load())offset+=n;
  }
};
static Message statusMessage,liveMessage,historyMessage;
inline void begin(const char* name){
 commands=xQueueCreateStatic(8,sizeof(Command),commandStorage,&commandControl);BLEDevice::init(name);BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);BLEDevice::setSecurityCallbacks(new Security());
 BLEDevice::setMTU(247);
 int bondCount=esp_ble_get_bond_device_num();
 if(bondCount==1){esp_ble_bond_dev_t bond;esp_ble_get_bond_device_list(&bondCount,&bond);memcpy(owner,bond.bd_addr,6);hasOwner=true;}
 BLESecurity* security=new BLESecurity();security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);security->setCapability(ESP_IO_CAP_OUT);security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK|ESP_BLE_ID_KEY_MASK);security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK|ESP_BLE_ID_KEY_MASK);
 server=BLEDevice::createServer();server->setCallbacks(new ServerCallbacks());BLEService* service=server->createService(BLEUUID(serviceUUID),24);
 auto notification=[&](const char* id){auto c=service->createCharacteristic(id,BLECharacteristic::PROPERTY_NOTIFY);c->setCallbacks(new DeliveryCallbacks());auto descriptor=new BLE2902();descriptor->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM|ESP_GATT_PERM_WRITE_ENC_MITM);c->addDescriptor(descriptor);return c;};
 statusChar=notification(statusUUID);liveChar=notification(liveUUID);historyChar=notification(historyUUID);
 controlChar=service->createCharacteristic(controlUUID,BLECharacteristic::PROPERTY_WRITE);controlChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENC_MITM);controlChar->setCallbacks(new ControlCallbacks());service->start();
 server->getAdvertising()->addServiceUUID(serviceUUID);server->getAdvertising()->setScanResponse(true);
 if(esp_ble_get_bond_device_num()>0)server->startAdvertising();
}
inline void pair(uint64_t now){if(esp_ble_get_bond_device_num()>0){Serial.println("PAIR refused: use forget-phone first to replace owner");return;}pairingUntil.store(now+120000);server->startAdvertising();Serial.println("PAIR window=120s; subscribe from iPhone; passkey will appear here");}
inline void forget(){
 ++generation;pairingUntil.store(0);authorized.store(false);server->getAdvertising()->stop();if(connected.load())server->disconnect(server->getConnId());
 int count=esp_ble_get_bond_device_num();if(count>0&&count<=8){esp_ble_bond_dev_t list[8];esp_ble_get_bond_device_list(&count,list);for(int i=0;i<count;++i)esp_ble_remove_bond_device(list[i].bd_addr);}
 portENTER_CRITICAL(&ownerLock);hasOwner=false;portEXIT_CRITICAL(&ownerLock);
}
inline void tick(uint64_t now){
 if(disconnected.exchange(false)){statusMessage.reset();liveMessage.reset();historyMessage.reset();if(esp_ble_get_bond_device_num()>0)server->startAdvertising();}
 if(pairingUntil.load()&&now>=pairingUntil.load()){pairingUntil.store(0);if(!connected.load()&&!esp_ble_get_bond_device_num())server->getAdvertising()->stop();}
 if(connected.load()&&!authorized.load()&&now-connectedAt.load()>30000&&now>=pairingUntil.load())server->disconnect(server->getConnId());
 static uint64_t last=0;static unsigned turn=0;if(!authorized.load()||now-last<2)return;last=now;++turn;
 if(statusMessage.pending())statusMessage.send(statusChar);
 else if(historyMessage.pending()&&(!(turn%4)||!liveMessage.pending()))historyMessage.send(historyChar);
 else if(liveMessage.pending())liveMessage.send(liveChar);
}
}
}
