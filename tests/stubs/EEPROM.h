#pragma once
#include <stdint.h>
#include <string.h>
struct EEPROMClass {
  uint8_t bytes[1024];
  int writes=0,failAfter=-1;bool ignoreWrites=false;
  EEPROMClass(){memset(bytes,255,sizeof(bytes));}
  int length(){return sizeof(bytes);}
  uint8_t read(int a){return bytes[a];}
  void update(int a,uint8_t b){
    if(failAfter==writes)throw 1;
    ++writes;if(!ignoreWrites)bytes[a]=b;
  }
};
extern EEPROMClass EEPROM;
