#pragma once
#include <stdint.h>

// Independent Part A board. Twenty CRC-protected records at 0..459, erase
// generation journal at 900..915, and a symptom count journal at 916..937.
struct PartASavedSession {
  uint32_t id,activeMs,historyId;
  uint16_t reps;
  uint8_t exercise;
};

struct PartASymptoms {
  uint16_t squats, lateralRaises;
  uint32_t total() const { return uint32_t(squats)+lateralRaises; }
};

template<class Memory> class PartAArchive {
  Memory& memory;
  static const int SYMPTOMS_BASE=916, SYMPTOM_BYTES=11;
  static void put(uint8_t* p,uint32_t n,uint8_t bytes){for(uint8_t i=0;i<bytes;++i){p[i]=uint8_t(n);n>>=8;}}
  static uint32_t get(const uint8_t* p,uint8_t bytes){uint32_t n=0;for(uint8_t i=0;i<bytes;++i)n|=uint32_t(p[i])<<(8*i);return n;}
  static uint16_t crc(const uint8_t* p,uint8_t size){
    uint16_t c=0xFFFF;
    for(uint8_t i=0;i<size;++i){c^=uint16_t(p[i])<<8;for(uint8_t j=0;j<8;++j)c=(c&0x8000)?uint16_t((c<<1)^0x1021):uint16_t(c<<1);}
    return c;
  }
  uint32_t generationAt(int address){
    uint8_t p[8];for(uint8_t i=0;i<8;++i)p[i]=memory.read(address+i);
    return p[0]==0xD8 && p[1]==1 && get(p+6,2)==crc(p+1,5)?get(p+2,4):0;
  }
  bool writeGeneration(uint32_t value){
    int at=generationAt(900)<=generationAt(908)?900:908;
    uint8_t p[8]={0,1};put(p+2,value,4);put(p+6,crc(p+1,5),2);
    memory.update(at,0);for(uint8_t i=1;i<8;++i)memory.update(at+i,p[i]);memory.update(at,0xD8);
    return generationAt(at)==value;
  }
  PartASymptoms symptomsAt(int address,uint32_t generation){
    uint8_t bytes[SYMPTOM_BYTES];
    for(uint8_t i=0;i<SYMPTOM_BYTES;++i)bytes[i]=memory.read(address+i);
    if(bytes[0]!=0xD9 || get(bytes+1,4)!=generation || get(bytes+9,2)!=crc(bytes+1,8))return {};
    return {uint16_t(get(bytes+5,2)),uint16_t(get(bytes+7,2))};
  }
public:
  static const uint8_t CAPACITY=20,RECORD_BYTES=23;
  explicit PartAArchive(Memory& m):memory(m){}
  uint32_t historyId(){if(memory.length()<916)return 0;uint32_t a=generationAt(900),b=generationAt(908);return a>b?a:b;}
  PartASymptoms symptoms(){
    if(memory.length()<SYMPTOMS_BASE+2*SYMPTOM_BYTES)return {};
    uint32_t generation=historyId();
    if(!generation)return {};
    PartASymptoms first=symptomsAt(SYMPTOMS_BASE,generation);
    PartASymptoms second=symptomsAt(SYMPTOMS_BASE+SYMPTOM_BYTES,generation);
    return first.total()>=second.total()?first:second;
  }
  bool addSymptom(uint8_t exercise){
    if(exercise>1 || memory.length()<SYMPTOMS_BASE+2*SYMPTOM_BYTES)return false;
    uint32_t generation=historyId();
    if(!generation){
      if(!writeGeneration(1))return false;
      generation=1;
    }
    PartASymptoms first=symptomsAt(SYMPTOMS_BASE,generation);
    PartASymptoms second=symptomsAt(SYMPTOMS_BASE+SYMPTOM_BYTES,generation);
    bool firstLatest=first.total()>=second.total();
    PartASymptoms next=firstLatest?first:second;
    uint16_t& count=exercise==0?next.squats:next.lateralRaises;
    if(count==UINT16_MAX)return false;
    ++count;
    int address=firstLatest?SYMPTOMS_BASE+SYMPTOM_BYTES:SYMPTOMS_BASE;
    uint8_t bytes[SYMPTOM_BYTES]={};
    put(bytes+1,generation,4);put(bytes+5,next.squats,2);put(bytes+7,next.lateralRaises,2);
    put(bytes+9,crc(bytes+1,8),2);
    // Keep the previous count valid until the new copy is fully written.
    memory.update(address,0);
    for(uint8_t i=1;i<SYMPTOM_BYTES;++i)memory.update(address+i,bytes[i]);
    memory.update(address,0xD9);
    PartASymptoms check=symptomsAt(address,generation);
    return check.squats==next.squats && check.lateralRaises==next.lateralRaises;
  }
  bool readSlot(uint8_t slot,PartASavedSession& r){
    if(slot>=CAPACITY || memory.length()<916)return false;
    uint8_t p[RECORD_BYTES];for(uint8_t i=0;i<RECORD_BYTES;++i)p[i]=memory.read(slot*RECORD_BYTES+i);
    if(p[0]!=0xD7 || p[1]!=1 || p[2]>1 || get(p+21,2)!=crc(p+1,20))return false;
    r.exercise=p[2];r.id=get(p+3,4);r.activeMs=get(p+7,4);r.reps=get(p+11,2);
    // Bytes 13..16 are reserved to preserve the existing EEPROM format.
    r.historyId=get(p+17,4);
    // A committed erase generation hides every older record immediately, even
    // if power disappears before all old bytes have been cleared.
    return r.id && r.historyId==historyId() && r.reps<=32767;
  }
  uint32_t latestId(){PartASavedSession r;uint32_t latest=0;for(uint8_t i=0;i<CAPACITY;++i)if(readSlot(i,r)&&r.id>latest)latest=r.id;return latest;}
  bool readId(uint32_t id,PartASavedSession& r){return id && readSlot(uint8_t((id-1)%CAPACITY),r) && r.id==id;}
  bool append(PartASavedSession& r){
    if(memory.length()<916 || r.exercise>1 || r.reps>32767)return false;
    uint32_t latest=latestId();if(latest==UINT32_MAX)return false;
    r.historyId=historyId();if(!r.historyId){if(!writeGeneration(1))return false;r.historyId=1;}
    r.id=latest+1;uint8_t p[RECORD_BYTES]={};p[1]=1;p[2]=r.exercise;
    put(p+3,r.id,4);put(p+7,r.activeMs,4);put(p+11,r.reps,2);
    put(p+17,r.historyId,4);put(p+21,crc(p+1,20),2);
    int at=int((r.id-1)%CAPACITY)*RECORD_BYTES;
    memory.update(at,0);for(uint8_t i=1;i<RECORD_BYTES;++i)memory.update(at+i,p[i]);memory.update(at,0xD7);
    PartASavedSession check;return readId(r.id,check);
  }
  bool clear(){
    if(memory.length()<916)return false;
    uint32_t old=historyId();
    if(old==UINT32_MAX || !writeGeneration(old+1))return false;
    // Commit the new generation first, so interrupted erases cannot bring back
    // old reports. Clear both report slots as well as the workout records.
    if(memory.length()>=SYMPTOMS_BASE+2*SYMPTOM_BYTES){
      for(int i=SYMPTOMS_BASE;i<SYMPTOMS_BASE+2*SYMPTOM_BYTES;++i)memory.update(i,0);
      for(int i=SYMPTOMS_BASE;i<SYMPTOMS_BASE+2*SYMPTOM_BYTES;++i)if(memory.read(i)!=0)return false;
    }
    for(int i=0;i<CAPACITY*RECORD_BYTES;++i)memory.update(i,0);
    for(int i=0;i<CAPACITY*RECORD_BYTES;++i)if(memory.read(i)!=0)return false;
    return historyId()==old+1;
  }
};
