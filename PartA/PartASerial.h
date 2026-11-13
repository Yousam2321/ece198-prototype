#pragma once
#include <Arduino.h>
#include <Print.h>

// USB CDC bulk writes in the R4 core can wait indefinitely after a host stops
// reading. Only submit one byte after checking available space; bound the wait
// outside the core so buttons/LCD do not depend on an open PC connection.
template<class Port> class PartASerialOutput : public Print {
  Port& port;
  bool writeFailed=false;
public:
  explicit PartASerialOutput(Port& p):port(p){}
  using Print::write;
  bool canWrite(int bytes){return bool(port) && port.availableForWrite()>=bytes;}
  void resetError(){writeFailed=false;}
  bool failed()const{return writeFailed;}
  size_t write(uint8_t value) override {
    uint32_t started=millis();
    while(bool(port) && port.availableForWrite()<1){
      if(uint32_t(millis()-started)>=20){writeFailed=true;return 0;}
      yield();
    }
    if(!bool(port) || port.availableForWrite()<1){writeFailed=true;return 0;}
    size_t sent=port.write(value); // One byte fits; never enter a bulk-write wait.
    if(sent!=1)writeFailed=true;
    return sent;
  }
};
