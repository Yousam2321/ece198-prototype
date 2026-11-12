#pragma once
#include <stdint.h>

// Base records remain at 0..419, preserving B2.3 histories. B2.4 adds metadata
// at 420..739 and a redundant archive-generation journal at 900..915.
struct SavedSession {
  uint32_t id, activeMs, inBandMs;
  uint16_t referenceMm, toleranceMm;
  uint8_t exercise;
  uint32_t trailingMs, historyId;
  bool tailKnown, hasHold;
};

template<class Memory> class SessionArchive {
  Memory& memory;
  static void put(uint8_t* p,uint32_t n,uint8_t bytes){
    for(uint8_t i=0;i<bytes;++i){p[i]=uint8_t(n);n>>=8;}
  }
  static uint32_t get(const uint8_t* p,uint8_t bytes){
    uint32_t n=0;for(uint8_t i=0;i<bytes;++i)n|=uint32_t(p[i])<<(8*i);return n;
  }
  static uint16_t crc(const uint8_t* p,uint8_t length,uint16_t c=0xFFFF){
    for(uint8_t i=0;i<length;++i){c^=uint16_t(p[i])<<8;
      for(uint8_t j=0;j<8;++j)c=(c&0x8000)?uint16_t((c<<1)^0x1021):uint16_t(c<<1);
    }return c;
  }
  uint32_t generationAt(int address){
    uint8_t p[8];for(uint8_t i=0;i<8;++i)p[i]=memory.read(address+i);
    return p[0]==0xB7 && p[1]==1 && get(p+6,2)==crc(p+1,5)?get(p+2,4):0;
  }
  bool writeGeneration(uint32_t value){
    uint32_t a=generationAt(900),b=generationAt(908);
    int address=a<=b?900:908;
    uint8_t p[8]={0,1};put(p+2,value,4);put(p+6,crc(p+1,5),2);
    memory.update(address,0);
    for(uint8_t i=1;i<8;++i)memory.update(address+i,p[i]);
    memory.update(address,0xB7);
    return generationAt(address)==value;
  }
public:
  static const uint8_t CAPACITY=20, RECORD_BYTES=21, EXTRA_BYTES=16;
  explicit SessionArchive(Memory& m):memory(m){}
  uint32_t historyId(){
    uint32_t a=generationAt(900),b=generationAt(908);return a>b?a:b;
  }
  // Chest-press saves share the generation ID, but have a separate record area.
  uint32_t ensureHistoryId(){
    uint32_t value=historyId();
    if(!value){if(!writeGeneration(1))return 0;value=1;}
    return value;
  }
  bool readSlot(uint8_t slot,SavedSession& r){
    if(slot>=CAPACITY || memory.length()<916)return false;
    uint8_t p[RECORD_BYTES];
    for(uint8_t i=0;i<RECORD_BYTES;++i)p[i]=memory.read(slot*RECORD_BYTES+i);
    if(p[0]!=0xA5 || (p[1]!=1 && p[1]!=2) || p[2]>1 || get(p+19,2)!=crc(p+1,18))return false;
    r.id=get(p+3,4);r.activeMs=get(p+7,4);r.inBandMs=get(p+11,4);
    r.referenceMm=get(p+15,2);r.toleranceMm=get(p+17,2);r.exercise=p[2];
    r.trailingMs=0;r.historyId=0;r.tailKnown=false;r.hasHold=false;
    if(!r.id || r.inBandMs>r.activeMs)return false;
    if(p[1]==2){
      uint8_t e[EXTRA_BYTES];int address=420+slot*EXTRA_BYTES;
      for(uint8_t i=0;i<EXTRA_BYTES;++i)e[i]=memory.read(address+i);
      uint16_t checksum=crc(e+1,13,crc(p+1,18));
      uint32_t tail=get(e+6,4);
      if(e[0]==0xA6 && (e[1]==1 || e[1]==3) && get(e+2,4)==r.id &&
         get(e+14,2)==checksum && tail<=r.activeMs && r.inBandMs<=r.activeMs-tail){
        r.trailingMs=tail;r.historyId=get(e+10,4);r.tailKnown=true;r.hasHold=e[1]&2;
      }
    }
    return true;
  }
  uint32_t latestId(){
    SavedSession r;uint32_t latest=0;
    for(uint8_t i=0;i<CAPACITY;++i)if(readSlot(i,r) && r.id>latest)latest=r.id;
    return latest;
  }
  bool readId(uint32_t id,SavedSession& r){
    return id && readSlot(uint8_t((id-1)%CAPACITY),r) && r.id==id;
  }
  bool clear(){
    if(memory.length()<916)return false;
    uint32_t old=historyId();if(old==UINT32_MAX || !writeGeneration(old+1))return false;
    for(int i=0;i<740;++i)memory.update(i,0);
    for(int i=0;i<740;++i)if(memory.read(i)!=0)return false;
    return true;
  }
  bool append(SavedSession& r){
    uint32_t last=latestId();
    if(last==UINT32_MAX || memory.length()<916)return false;
    if(r.tailKnown && (r.trailingMs>r.activeMs || r.inBandMs>r.activeMs-r.trailingMs))return false;
    r.historyId=historyId();
    if(!r.historyId){if(!writeGeneration(1))return false;r.historyId=1;}
    r.id=last+1;
    uint8_t p[RECORD_BYTES]={};p[1]=r.tailKnown?2:1;p[2]=r.exercise;
    put(p+3,r.id,4);put(p+7,r.activeMs,4);put(p+11,r.inBandMs,4);
    put(p+15,r.referenceMm,2);put(p+17,r.toleranceMm,2);put(p+19,crc(p+1,18),2);
    int slot=int((r.id-1)%CAPACITY),address=slot*RECORD_BYTES,extra=420+slot*EXTRA_BYTES;
    memory.update(address,0);memory.update(extra,0);
    for(uint8_t i=1;i<RECORD_BYTES;++i)memory.update(address+i,p[i]);
    if(r.tailKnown){
      uint8_t e[EXTRA_BYTES]={};e[1]=r.hasHold?3:1;
      put(e+2,r.id,4);put(e+6,r.trailingMs,4);put(e+10,r.historyId,4);
      put(e+14,crc(e+1,13,crc(p+1,18)),2);
      for(uint8_t i=1;i<EXTRA_BYTES;++i)memory.update(extra+i,e[i]);
      memory.update(extra,0xA6);
    }
    memory.update(address,0xA5);
    SavedSession check;
    return readId(r.id,check) && (!r.tailKnown || check.tailKnown);
  }
};
