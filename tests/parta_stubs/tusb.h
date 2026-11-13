#pragma once
#include <cassert>
#include "Arduino.h"
extern bool fakeUsbInterruptsEnabled;
extern bool fakeUsbCloseOnSubmit;
extern bool fakeUsbFullOnSubmit;
inline bool tud_cdc_connected(){return Serial.connected;}
inline uint32_t tud_cdc_write_available(){return uint32_t(Serial.space);}
inline uint32_t tud_cdc_write_char(char value){
  assert(!fakeUsbInterruptsEnabled);
  if(!Serial.connected || Serial.space<1)return 0;
  Serial.output+=value;return 1;
}
inline uint32_t tud_cdc_write_flush(){return 0;} // Endpoint can remain busy.
