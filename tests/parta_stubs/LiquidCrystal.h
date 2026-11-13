#pragma once
#include <sstream>
#include <string>
struct LiquidCrystal {
  std::string rows[2];int column=0,line=0;
  LiquidCrystal(int,int,int,int,int,int){}
  void begin(int,int){clear();}
  void clear(){rows[0]=rows[1]=std::string(16,' ');column=line=0;}
  void setCursor(int c,int r){column=c;line=r;}
  void print(const __FlashStringHelper* value){print(reinterpret_cast<const char*>(value));}
  template<class T>void print(T value){
    std::ostringstream s;s<<value;
    for(char ch:s.str()){if(line>=0 && line<2 && column>=0 && column<16)rows[line][column]=ch;++column;}
  }
};
