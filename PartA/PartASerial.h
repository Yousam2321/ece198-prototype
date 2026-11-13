#pragma once
#include <Arduino.h>
#if defined(ARDUINO_ARCH_RENESAS_UNO)
#include <tusb.h>
#include <device/usbd_pvt.h>
#endif

// Bounded transport only: no Print calls, no waiting for USB space, and no
// Serial boolean conversion. A foreground export advances a little each loop.
template<class Port> class PartASerialLink {
  Port& port;
public:
  explicit PartASerialLink(Port& p):port(p){}
  bool connected(){
#if defined(ARDUINO_ARCH_RENESAS_UNO)
    return tud_cdc_connected();
#else
    return true; // The R3 UART cannot detect whether its USB host is open.
#endif
  }
  int available(){return connected()?port.available():0;}
  int read(){return port.read();} // Caller checks the int result before casting.
  uint8_t writeSome(const char* data,uint8_t length){
    if(!length || !connected())return 0;
#if defined(ARDUINO_ARCH_RENESAS_UNO)
    // USB interrupts are masked only for this bounded queue operation.
    usbd_int_set(false);
    uint8_t count=0;
    if(tud_cdc_connected()){
      uint32_t room=tud_cdc_write_available();
      count=uint8_t(room<length?room:length);
      if(count)count=uint8_t(tud_cdc_write(data,count));
      if(count)tud_cdc_write_flush();
    }
    usbd_int_set(true);
    return count;
#else
    int room=port.availableForWrite();
    uint8_t count=room>0?uint8_t(room<length?room:length):0;
    for(uint8_t i=0;i<count;++i)if(port.write(uint8_t(data[i]))!=1)return i;
    return count;
#endif
  }
};
