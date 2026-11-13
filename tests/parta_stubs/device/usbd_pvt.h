#pragma once
#include "tusb.h"
inline void usbd_int_set(bool enabled){
  fakeUsbInterruptsEnabled=enabled;
  if(!enabled && fakeUsbCloseOnSubmit){Serial.connected=false;fakeUsbCloseOnSubmit=false;}
  if(!enabled && fakeUsbFullOnSubmit){Serial.space=0;fakeUsbFullOnSubmit=false;}
}
