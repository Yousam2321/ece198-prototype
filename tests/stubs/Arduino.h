#pragma once
#include <stdint.h>
#include <stdio.h>
#include <sstream>
#include <iomanip>
#include <string>
struct __FlashStringHelper;
#define F(s) reinterpret_cast<const __FlashStringHelper*>(s)
const int HIGH=1, LOW=0, INPUT_PULLUP=2, OUTPUT=3, INPUT=4, LED_BUILTIN=13;
extern uint32_t fakeNow;
extern uint32_t fakePulse;
extern int fakePins[20];
inline uint32_t millis(){return fakeNow;}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline int digitalRead(int p){return fakePins[p];}
inline void delayMicroseconds(int){}
inline uint32_t pulseIn(int,int,uint32_t){return fakePulse;}
struct FakeSerial {
  void begin(int){}
  std::string output;
  void println(){output+='\n';}
  int available(){return 0;}
  char read(){return 0;}
  void print(const __FlashStringHelper* p){output+=reinterpret_cast<const char*>(p);}
  template<class T>void print(T value){std::ostringstream s;s<<value;output+=s.str();}
  template<class T>void print(T value,int places){std::ostringstream s;s<<std::fixed<<std::setprecision(places)<<value;output+=s.str();}
  template<class T>void println(T value){print(value);println();}
  template<class T>void println(T value,int places){print(value,places);println();}
};
extern FakeSerial Serial;
