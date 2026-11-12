#pragma once
#include <stdint.h>

// Ten chest-press records in EEPROM bytes 740..899 and 916..1015.
// Existing hold records (0..739) and generation journal (900..915) are untouched.
// No fields are re-used as fake in-band durations. The PC app is unchanged.
struct SavedRepSession {
  uint32_t id,activeMs,historyId;
  uint16_t good,attempts,startMm,pressedMm,lastTravelMm;
};

template<class Memory> class RepSessionArchive {
  Memory& memory;
  static int address(int offset){return offset<160?740+offset:916+offset-160;}
  static void put(uint8_t* p,uint32_t n,uint8_t size){
    for(uint8_t i=0;i<size;++i){p[i]=uint8_t(n);n>>=8;}
  }
  static uint32_t get(const uint8_t* p,uint8_t size){
    uint32_t n=0;for(uint8_t i=0;i<size;++i)n|=uint32_t(p[i])<<(8*i);return n;
  }
  static uint16_t crc(const uint8_t* p){
    uint16_t c=0xFFFF;
    for(uint8_t i=1;i<24;++i){c^=uint16_t(p[i])<<8;
      for(uint8_t j=0;j<8;++j)c=(c&0x8000)?uint16_t((c<<1)^0x1021):uint16_t(c<<1);
    }return c;
  }
public:
  static const uint8_t CAPACITY=10,RECORD_BYTES=26;
  explicit RepSessionArchive(Memory& m):memory(m){}
  bool readSlot(uint8_t slot,SavedRepSession& r){
    if(slot>=CAPACITY || memory.length()<1016)return false;
    uint8_t p[RECORD_BYTES];
    for(uint8_t i=0;i<RECORD_BYTES;++i)p[i]=memory.read(address(slot*RECORD_BYTES+i));
    if(p[0]!=0xC7 || p[1]!=1 || get(p+24,2)!=crc(p))return false;
    r.id=get(p+2,4);r.activeMs=get(p+6,4);r.good=get(p+10,2);r.attempts=get(p+12,2);
    r.startMm=get(p+14,2);r.pressedMm=get(p+16,2);r.lastTravelMm=get(p+18,2);r.historyId=get(p+20,4);
    int travel=int(r.startMm)-int(r.pressedMm);if(travel<0)travel=-travel;
    return r.id && r.historyId && r.good<=r.attempts && travel>=100;
  }
  uint32_t latestId(){
    SavedRepSession r;uint32_t latest=0;
    for(uint8_t i=0;i<CAPACITY;++i)if(readSlot(i,r) && r.id>latest)latest=r.id;
    return latest;
  }
  bool readId(uint32_t id,SavedRepSession& r){
    return id && readSlot(uint8_t((id-1)%CAPACITY),r) && r.id==id;
  }
  bool append(SavedRepSession& r){
    uint32_t last=latestId();
    int travel=int(r.startMm)-int(r.pressedMm);if(travel<0)travel=-travel;
    if(last==UINT32_MAX || memory.length()<1016 || !r.historyId || r.good>r.attempts || travel<100)return false;
    r.id=last+1;
    uint8_t p[RECORD_BYTES]={};p[1]=1;
    put(p+2,r.id,4);put(p+6,r.activeMs,4);put(p+10,r.good,2);put(p+12,r.attempts,2);
    put(p+14,r.startMm,2);put(p+16,r.pressedMm,2);put(p+18,r.lastTravelMm,2);put(p+20,r.historyId,4);
    put(p+24,crc(p),2);
    int offset=int((r.id-1)%CAPACITY)*RECORD_BYTES;
    memory.update(address(offset),0); // Invalidate, write, then commit last.
    for(uint8_t i=1;i<RECORD_BYTES;++i)memory.update(address(offset+i),p[i]);
    memory.update(address(offset),0xC7);
    SavedRepSession check;return readId(r.id,check);
  }
  bool clear(){
    if(memory.length()<1016)return false;
    for(int i=0;i<CAPACITY*RECORD_BYTES;++i)memory.update(address(i),0);
    for(int i=0;i<CAPACITY*RECORD_BYTES;++i)if(memory.read(address(i))!=0)return false;
    return true;
  }
};
