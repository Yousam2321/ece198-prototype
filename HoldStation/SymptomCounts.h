#pragma once
#include <stdint.h>

// Two copies in the eight free EEPROM bytes. Commit the marker byte last.
template<class Memory> class SymptomCounts {
  Memory& memory;
  static const uint32_t MARKER = 0x40000000UL;
  static const uint32_t MASK = 1023;
  uint32_t read(int slot) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value |= uint32_t(memory.read(1016+4*slot+i)) << (8*i);
    return (value >> 30) == 1 ? value : 0;
  }
public:
  explicit SymptomCounts(Memory& m):memory(m){}
  uint32_t snapshot() {
    if(memory.length()<1024)return 0;
    uint32_t a=read(0),b=read(1);
    return a>=b?a:b;
  }
  static unsigned count(uint32_t snapshot,uint8_t exercise) {
    return (snapshot >> (10*exercise)) & MASK;
  }
  bool add(uint8_t exercise) {
    if(exercise>2 || memory.length()<1024)return false;
    uint32_t a=read(0),b=read(1),current=a>=b?a:b;
    if(count(current,exercise)==MASK)return false;
    uint32_t next=(current|MARKER)+(1UL<<(10*exercise));
    int slot=a>=b?1:0,address=1016+4*slot;
    memory.update(address+3,0);
    for(int i=0;i<4;++i)memory.update(address+i,uint8_t(next>>(8*i)));
    return read(slot)==next;
  }
  bool clear() {
    if(memory.length()<1024)return false;
    memory.update(1019,0);memory.update(1023,0);
    return snapshot()==0;
  }
};
