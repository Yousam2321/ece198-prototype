#pragma once
const int I2C_SPEED_STANDARD=100000;
extern bool fakeSensorPresent;
extern long fakeIr;
struct MAX30105 {
  template<class W>bool begin(W&,int){return fakeSensorPresent;}
  void setup(){} void setPulseAmplitudeRed(int){} void setPulseAmplitudeIR(int){} void setPulseAmplitudeGreen(int){}
  long getIR(){return fakeIr;}
};
