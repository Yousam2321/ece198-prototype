#pragma once
#include <cassert>
#include "Arduino.h"
extern bool fakeUsbInterruptsEnabled;
extern bool fakeUsbCloseOnSubmit;
extern bool fakeUsbFullOnSubmit;
inline bool tud_cdc_connected(){return Serial.connected;}
inline uint32_t tud_cdc_write_available(){return uint32_t(Serial.space);}
inline uint32_t tud_cdc_write(const void* data,uint32_t length){
  assert(!fakeUsbInterruptsEnabled);
  if(!Serial.connected || Serial.space<1)return 0;
  uint32_t count=length<uint32_t(Serial.space)?length:uint32_t(Serial.space);
  Serial.output.append(static_cast<const char*>(data),count);return count;
}
inline uint32_t tud_cdc_write_flush(){return 0;} // Endpoint can remain busy.
