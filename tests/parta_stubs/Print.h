#pragma once
#include <sstream>
#include <iomanip>
#include <string>
class Print {
public:
  virtual ~Print(){}
  virtual size_t write(uint8_t)=0;
  size_t write(const uint8_t* data,size_t size){
    size_t count=0;while(count<size && write(data[count]))++count;return count;
  }
  size_t print(const __FlashStringHelper* value){return print(reinterpret_cast<const char*>(value));}
  template<class T>size_t print(T value){std::ostringstream s;s<<value;return text(s.str());}
  template<class T>size_t print(T value,int places){std::ostringstream s;s<<std::fixed<<std::setprecision(places)<<value;return text(s.str());}
  size_t println(){return text("\r\n");}
  template<class T>size_t println(T value){size_t n=print(value);return n+println();}
  template<class T>size_t println(T value,int places){size_t n=print(value,places);return n+println();}
private:
  size_t text(const std::string& value){return write(reinterpret_cast<const uint8_t*>(value.data()),value.size());}
};
