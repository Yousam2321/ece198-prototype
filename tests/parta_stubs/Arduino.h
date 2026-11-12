#pragma once
#include "../stubs/Arduino.h"
using byte=uint8_t;
const int A0=0,A1=1,A2=2;
extern int fakeAnalog[3];
inline int analogRead(int p){return fakeAnalog[p];}
inline void delay(unsigned long ms){fakeNow+=uint32_t(ms);}
