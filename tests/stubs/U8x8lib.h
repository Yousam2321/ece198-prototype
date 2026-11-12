#pragma once
#include <stdint.h>
#include <string.h>
const int U8X8_PIN_NONE=255,u8x8_font_chroma48medium8_r=0;
extern unsigned int fakeOledWrites;
extern char fakeScreen[8][17];
struct FakeOLED {
  explicit FakeOLED(int){}
  void setI2CAddress(uint8_t){}
  void begin(){}
  void setFont(int){}
  void drawString(int,uint8_t row,const char* text){
    ++fakeOledWrites;strncpy(fakeScreen[row],text,16);fakeScreen[row][16]='\0';
  }
};
using U8X8_SH1106_128X64_NONAME_HW_I2C=FakeOLED;
using U8X8_SSD1306_128X64_NONAME_HW_I2C=FakeOLED;
