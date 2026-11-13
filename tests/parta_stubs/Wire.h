#pragma once
#define WIRE_HAS_TIMEOUT 1
extern uint8_t fakeWireError;
struct PartAWire {
  unsigned timeout=0;
  void begin(){}
  void setWireTimeout(unsigned value,bool){timeout=value;}
  void beginTransmission(uint8_t){}
  uint8_t endTransmission(){return fakeWireError;}
};
extern PartAWire Wire;
