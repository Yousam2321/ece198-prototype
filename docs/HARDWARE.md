# Current hardware reference

| Unit | Software | Pins |
|---|---|---|
| Part A | `PartA/PartA.ino` | START D2, EXERCISE D3; analog X/Y/Z A0/A1/A2; LCD RS/E/D4–D7 D7/D8/D9–D12; pulse I2C SDA/SCL |
| Part B | `HoldStation/HoldStation.ino` | GO D2, MODE D3; HC-SR04 TRIG D8/ECHO D9; OLED SDA A4/SCL A5 |

These are separate boards: do not combine their wiring or upload one sketch over the other. Buttons connect to GND when pressed and use internal pull-ups. Keep each unit's existing working power wiring and common ground. No shield or new LED is required for this update.

Part A preserves the uploaded team's LCD/accelerometer/pulse setup. See [PART_A.md](PART_A.md) for exact original mapping and tests. Use the board's labeled SDA/SCL pins; check each breakout's documented voltage interface before changing its power wiring.

Part B runs on the 5 V Uno R3. HC-SR04 VCC goes to 5 V, GND to GND. The shown four-pin OLED labels are GND/VCC/SCL/SDA; use those labels rather than assuming another module's pin order. Default driver is SH1106 at I2C address 0x3C; `HoldStation/Config.h` selects SSD1306 if appropriate. U8g2 supplies U8x8lib.h. No Grove shield is required.

Recalibrate Part B after moving the sensor or target. Hold exercises use proximity to a calibrated reference; chest press uses two points along the hand/target trajectory. Sensor readings do not establish complete body posture.
