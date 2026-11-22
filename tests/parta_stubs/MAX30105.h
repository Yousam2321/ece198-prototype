#pragma once
const int I2C_SPEED_STANDARD=100000;
extern bool fakeSensorPresent;
extern long fakeIr;
struct MAX30105 {
  bool buffered=false;
  int ledMode=0;
  template<class W>bool begin(W&,int){return fakeSensorPresent;}
  void setup(int,int,int channels){ledMode=channels;}
  long getIR(){throw 6;} // The rewritten loop must never use the 250 ms wait.
  uint16_t check(){buffered=true;return 1;}
  uint8_t available(){return buffered?1:0;}
  long getFIFOIR(){return fakeIr;}
  void nextSample(){buffered=false;}
};
