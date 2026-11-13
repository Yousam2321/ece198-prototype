#pragma once
#include <Arduino.h>
#include <Print.h>
#if defined(ARDUINO_ARCH_RENESAS_UNO)
#include <tusb.h>
#include <device/usbd_pvt.h>
#endif

// R4 SerialUSB.write() contains an unbounded wait, even for a single byte if
// space changes after the caller's check. Bypass that writer on native R4 USB.
// Queue one byte with USB IRQs briefly masked and always restore them; never
// wait for host activity while IRQs are masked. R3 keeps its normal UART path.
template<class Port> class PartASerialOutput : public Print {
  Port& port;
  bool writeFailed=false;
  bool connected(){
#if defined(ARDUINO_ARCH_RENESAS_UNO)
    return tud_cdc_connected();
#else
    return bool(port);
#endif
  }
  int space(){
#if defined(ARDUINO_ARCH_RENESAS_UNO)
    return int(tud_cdc_write_available());
#else
    return port.availableForWrite();
#endif
  }
  size_t submit(uint8_t value){
#if defined(ARDUINO_ARCH_RENESAS_UNO)
    usbd_int_set(false);
    size_t sent=0;
    if(tud_cdc_connected() && tud_cdc_write_available()>0){
      sent=tud_cdc_write_char(char(value));
      if(sent)tud_cdc_write_flush(); // Starts a transfer; does not wait for host.
    }
    usbd_int_set(true);
    return sent;
#else
    return port.write(value);
#endif
  }
public:
  explicit PartASerialOutput(Port& p):port(p){}
  using Print::write;
  bool canWrite(int bytes){return connected() && space()>=bytes;}
  void resetError(){writeFailed=false;}
  bool failed()const{return writeFailed;}
  size_t write(uint8_t value) override {
    uint32_t started=millis();
    while(connected() && space()<1){
      if(uint32_t(millis()-started)>=20){writeFailed=true;return 0;}
      yield();
    }
    if(!connected() || space()<1){writeFailed=true;return 0;}
    size_t sent=submit(value);
    if(sent!=1)writeFailed=true;
    return sent;
  }
};
