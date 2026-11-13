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
  bool connected=true;int space=64;std::string input;
  operator bool(){return connected;}
  int availableForWrite(){return space;}
  size_t write(uint8_t value){
    if(!connected || space<1)throw 2;
    output+=char(value);return 1;
  }
  void println(){output+='\n';}
  int available(){return int(input.size());}
  char read(){char value=input[0];input.erase(0,1);return value;}
  void print(const __FlashStringHelper* p){output+=reinterpret_cast<const char*>(p);}
  template<class T>void print(T value){std::ostringstream s;s<<value;output+=s.str();}
  template<class T>void print(T value,int places){std::ostringstream s;s<<std::fixed<<std::setprecision(places)<<value;output+=s.str();}
  template<class T>void println(T value){print(value);println();}
  template<class T>void println(T value,int places){print(value,places);println();}
};
extern FakeSerial Serial;
