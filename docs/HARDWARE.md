# Hardware used in this repository

Use the full schematic pages and four owner checklists in Team_Build_and_Test_Plan.pdf.
The actual accelerometer is **MMA7361**, not the initially assumed Grove ADXL335.

## Wearable A - Uno R4 Minima

| MMA7361 pin | Connection |
|---|---|
| 3V3 and Sleep / SL | Arduino 3.3V |
| GND, G-Select / GS, Selftest / ST | GND |
| X / Y / Z | A0 / A1 / A2 |
| Module 5V and 0g-Detect | Leave open in this 3.3V-powered setup |

Wearable bare button: D2 to GND through normally open contacts. Internal pull-up is enabled.
Do not drive the accelerometer control pins with 5V.

## GY-MAX30102

The photographed board is the purple five-pin GY-MAX30102. Its back is consistent
with onboard regulation/level shifting, but its exact interface remains a hardware check.
The team plan shows proposed VIN=5V, GND=GND, SDA=A4, SCL=A5 and INT unconnected
**for a compatible regulated/level-shifted module**. Follow its power/interface checks
before connecting data wires to a 5V host. Add an appropriate level shifter if required.
Keep ENABLE_MAX30102=0 until that check is complete. Once verified, set 1 in Config.h.

## Station B - Uno R3

| Part | Connection |
|---|---|
| HC-SR04 VCC / GND | 5V / GND |
| HC-SR04 TRIG / ECHO | D8 / D9 |
| GO / MODE bare buttons | D2 / D3 to GND when pressed |
| OLED SDA / SCL | A4 / A5 |
| OLED GND / VCC | GND / verified module supply |

The photographed OLED header reads GND, VCC, SCL, SDA. Its controller and voltage
rating are not established by the image. Use ENABLE_OLED=0 for the first Serial-only
build if electrical compatibility is unconfirmed. Try SH1106 then SSD1306 driver settings;
0x78/0x7A printed addresses correspond to 7-bit 0x3C/0x3D.

Each unit has its own power rails; do not link 5V and 3.3V. USB powers the Arduinos.
All devices within one unit share that unit's ground. Keep module pins on separate
breadboard connection groups and check any split power rails.
