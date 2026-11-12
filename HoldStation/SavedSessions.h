#pragma once
#include <stdint.h>

// Twenty summaries in EEPROM addresses 0..419. The oldest is overwritten.
// No personal data or clock date: this board has no real-time clock.
struct SavedSession {
  uint32_t id, activeMs, inBandMs;
  uint16_t referenceMm, toleranceMm;
  uint8_t exercise;
};

template<class Memory> class SessionArchive {
  Memory& memory;
  static uint16_t crc(const uint8_t* p) {
    uint16_t c=0xFFFF;
    for(uint8_t i=1;i<=18;++i){
      c^=uint16_t(p[i])<<8;
      for(uint8_t j=0;j<8;++j)c=(c&0x8000)?uint16_t((c<<1)^0x1021):uint16_t(c<<1);
    }
    return c;
  }
  static void put(uint8_t* p,uint32_t n,uint8_t bytes){
    for(uint8_t i=0;i<bytes;++i){p[i]=uint8_t(n);n>>=8;}
  }
  static uint32_t get(const uint8_t* p,uint8_t bytes){
    uint32_t n=0;for(uint8_t i=0;i<bytes;++i)n|=uint32_t(p[i])<<(8*i);return n;
  }
public:
  static const uint8_t CAPACITY=20, RECORD_BYTES=21;
  explicit SessionArchive(Memory& m):memory(m){}
  bool readSlot(uint8_t slot,SavedSession& r){
    if(slot>=CAPACITY || memory.length()<CAPACITY*RECORD_BYTES)return false;
    uint8_t p[RECORD_BYTES];
    for(uint8_t i=0;i<RECORD_BYTES;++i)p[i]=memory.read(slot*RECORD_BYTES+i);
    if(p[0]!=0xA5 || p[1]!=1 || p[2]>1 || get(p+19,2)!=crc(p))return false;
    r.id=get(p+3,4);r.activeMs=get(p+7,4);r.inBandMs=get(p+11,4);
    r.referenceMm=get(p+15,2);r.toleranceMm=get(p+17,2);r.exercise=p[2];
    return r.id>0 && r.inBandMs<=r.activeMs;
  }
  uint32_t latestId(){
    SavedSession r;uint32_t latest=0;
    for(uint8_t i=0;i<CAPACITY;++i)if(readSlot(i,r) && r.id>latest)latest=r.id;
    return latest;
  }
  bool readId(uint32_t id,SavedSession& r){
    return id && readSlot(uint8_t((id-1)%CAPACITY),r) && r.id==id;
  }
  // Erase only this archive's EEPROM range.
  bool clear(){
    if(memory.length()<CAPACITY*RECORD_BYTES)return false;
    for(int i=0;i<CAPACITY*RECORD_BYTES;++i)memory.update(i,0);
    for(int i=0;i<CAPACITY*RECORD_BYTES;++i)if(memory.read(i)!=0)return false;
    return true;
  }
  bool append(SavedSession& r){
    uint32_t last=latestId();
    if(last==UINT32_MAX || memory.length()<CAPACITY*RECORD_BYTES)return false;
    r.id=last+1;
    uint8_t p[RECORD_BYTES]={};p[1]=1;p[2]=r.exercise;
    put(p+3,r.id,4);put(p+7,r.activeMs,4);put(p+11,r.inBandMs,4);
    put(p+15,r.referenceMm,2);put(p+17,r.toleranceMm,2);put(p+19,crc(p),2);
    int address=int((r.id-1)%CAPACITY)*RECORD_BYTES;
    // Invalidate first; commit marker is written last. A partial record is ignored.
    memory.update(address,0);
    for(uint8_t i=1;i<RECORD_BYTES;++i)memory.update(address+i,p[i]);
    memory.update(address,0xA5);
    SavedSession check;return readId(r.id,check);
  }
};
