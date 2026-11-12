#pragma once
struct LiquidCrystal {
  LiquidCrystal(int,int,int,int,int,int){}
  void begin(int,int){} void clear(){} void setCursor(int,int){}
  template<class T>void print(T){}
};
