// Upload separately for first wiring checks. Do not connect an unverified I2C
// breakout just to run the scanner: it cannot establish electrical safety.
#include <Wire.h>
void setup(){
  Serial.begin(115200);
  unsigned long at=millis();while(!Serial && millis()-at<1500){}
  Wire.begin();
  Serial.println("I2C scan: 7-bit addresses");
  for(byte a=8;a<120;a++){
    Wire.beginTransmission(a);
    if(Wire.endTransmission()==0){Serial.print("Found 0x");Serial.println(a,HEX);}
  }
  Serial.println("Expected OLED: 0x3C or 0x3D; MAX30102: 0x57");
  Serial.println("A0,A1,A2,A3 (10-bit ADC)");
  #if defined(ARDUINO_ARCH_RENESAS_UNO)
  analogReadResolution(10);
  #endif
}
void loop(){
  Serial.print(analogRead(A0));Serial.print(',');
  Serial.print(analogRead(A1));Serial.print(',');
  Serial.print(analogRead(A2));Serial.print(',');Serial.println(analogRead(A3));
  delay(200);
}
