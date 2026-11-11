# ECE 198 Group 23 - hybrid exercise-monitor prototype

This package contains two Arduino sketches, direct-wiring schematics, a build guide,
a serial logger and tests. Use the **large breadboard for B**, and the **small
breadboard for A**. A Grove shield is not required.

Start with `BUILD_GUIDE.pdf`. Assemble and test **HoldStation first**: it gives a
clear wall-sit/plank demonstration with the OLED and HC-SR04. Then build the wearable.

## Controllers and files

| Unit | Controller assumed from your pitch | Open in Arduino IDE |
|---|---|---|
| A: wearable | Arduino Uno R4 Minima | `WearableMonitor/WearableMonitor.ino` |
| B: station | Arduino Uno R3 | `HoldStation/HoldStation.ino` |
| Diagnostic | Whichever board is being checked | `tools/SerialDiagnostics/SerialDiagnostics.ino` |

Keep each `.ino` in its matching folder with `PrototypeCore.h`. Do not upload
both main sketches to the same Arduino: uploading replaces the existing program.
There is no radio link between the two units in this version. Each works separately;
USB connects each to a laptop for logging. Standalone USB power does not save logs.

## Direct wiring - A

| Component pin | Uno R4 Minima |
|---|---|
| MMA7361 **3V3** | Arduino **3.3V** |
| MMA7361 **Sleep / SL** | Arduino **3.3V** (keeps sensor awake) |
| MMA7361 GND / G-Select (GS) / Selftest (ST) | GND (normal operation; +/-1.5g range) |
| MMA7361 X / Y / Z | A0 / A1 / A2 |
| MMA7361 **5V / 0g-Detect** | Leave unconnected in this 3.3V-powered setup |
| Optional potentiometer outer terminals | 5V and GND |
| Optional potentiometer centre/wiper | A3 |
| Bare pushbutton, contact 1 / contact 2 | D2 / GND |

The button uses `INPUT_PULLUP`; no external resistor is needed. Use the breakout's
printed labels, not an assumed left-to-right order. Reversing the potentiometer's
outer terminals only reverses menu direction. Your photographed potentiometer is
marked **B500K (500 kOhm)**. It is optional: **USE_SELECTOR_POT defaults to 0** and
profiles are chosen with `select 0` through `select 8` in Serial Monitor. Leave
it disconnected for the first build. A high-resistance divider can be less stable
when sampled by an ADC. For an optional knob, a 10 kOhm potentiometer is preferable;
if trying B500K, a 100 nF capacitor from A3 to GND helps provide sampling charge.
Enable USE_SELECTOR_POT=1 and check stable profile selection; return to Serial-only
selection if it jitters. The code includes repeated reads, but this is not proof
of accuracy for a 500 kOhm source. USB powers the Arduino initially.

The MMA7361's labelled 5V pin is a separate regulator-input option; it is not used
in this wiring. Do not connect it alongside the Arduino 3.3V supply. Never drive
Sleep, GS or ST with a 5V GPIO. No MMA7361 library is needed: X/Y/Z are analog signals.

### Optional MAX30102: electrical check before connection

Heart-rate code is included but `ENABLE_MAX30102` defaults to `0`. The generic
GY-MAX30102 name does not establish the breakout's supply and I2C level circuitry.
The sensor-chip datasheet is not a breakout-board schematic. Confirm the exact
breakout's VIN requirements and its compatibility with a **5V I2C host** before
connecting it. Do not copy the SparkFun MAX30105 board's power instructions onto
an unidentified GY board. A suitable bidirectional I2C level shifter may be needed;
that part is not assumed to be in your kit.

Once the breakout interface is verified:

| Connection | Uno R4 Minima |
|---|---|
| SDA, through level shifting if required | A4 / SDA |
| SCL, through level shifting if required | A5 / SCL |
| GND | GND |
| VIN | Verified breakout supply (not an assumed 5V) |
| INT / RD / IRD | Leave unconnected |

Install **SparkFun MAX3010x Pulse and Proximity Sensor Library**; set
`ENABLE_MAX30102` to `1`, then compile/upload. The library header/class is called
`MAX30105` even when used with MAX30102. No SpO2 calculation is implemented.

## Direct wiring - B

| Component pin | Uno R3 |
|---|---|
| HC-SR04 VCC / GND | 5V / GND |
| HC-SR04 TRIG / ECHO | D8 / D9 |
| OLED GND / VCC | GND / module-rated supply (5V only if module is rated for it) |
| OLED SCL / SDA | A5 / A4 (with level shifting if the module requires it) |
| GO bare button | D2 to GND |
| MODE bare button | D3 to GND |

Your OLED's front header is **GND, VCC, SCL, SDA**. Its controller is not identified
by the photographs. The code defaults to the SH1106 128x64 driver and includes
an SSD1306 alternative (`OLED_SH1106=0`). Its printed `0x78/0x7A` choices are
8-bit addresses; the 7-bit addresses are `0x3C/0x3D`. Start with `OLED_ADDRESS=0x3C`.
The code shifts the address when passing it to U8x8; do not change it to 0x78.
Confirm your module's voltage rating before powering it. If unknown, start the
station without the OLED (`ENABLE_OLED=0`) and use Serial Monitor.

Install **U8g2 by oliver** in Library Manager. This sketch uses its U8x8 text mode
to fit comfortably on the Uno R3. No extra ultrasonic library is required.

## Breadboard connections

- On each numbered row, a-e form one connected group and f-j form another.
  They are separated by the central channel. Different numbered rows are separate.
- An inline sensor/display header must have each electrical pin in a **different
  row/group**. Putting GND, VCC, SCL and SDA across a-e on one row shorts them.
- Power rails may be split. Use a continuity check; bridge + to + and - to -
  where needed. Never bridge + to -.
- Four-leg tactile buttons contain internally connected pairs. Use two contacts
  that become connected only while pressing, verified with a meter if uncertain.
- Within each unit, all components share its Arduino GND. Keep the two boards'
  power rails separate. There are no inter-board signal wires in this version.
- Unplug USB before changing wiring. Power each Arduino through its USB socket.
  Do not put the pitch's 6V button-cell pack on a 5V pin or on the breadboard 5V rail.

## First upload

1. Install Arduino IDE. In Boards Manager install **Arduino AVR Boards** for the R3
   and **Arduino UNO R4 Boards** for the R4 Minima.
2. Install **U8g2**; install SparkFun's MAX3010x library only when enabling HR.
3. Select the board and its COM port; choose the matching main sketch.
4. Upload and open Serial Monitor at **115200 baud**, with **Newline** line ending.
5. Type `help`. DATA/EVENT/SUMMARY lines are real device output, not simulated data.
   A reset or power loss clears session totals and calibrations.

## B: wall sit / plank operation

1. Secure the HC-SR04 on a stand. Keep its face clear. Aim at a broad target on the
   participant's torso/hip, with a reference distance between **12 and 150cm**.
   For plank, the sensor can sit on a support facing the underside/side of the
   torso; for wall sit, use a fixed stand aimed at the torso from the side/front.
   The target must remain in the beam; do not aim at a wall behind the person.
2. Tap MODE to choose WALL SIT or PLANK while IDLE. Changing mode invalidates calibration.
3. Hold the reference position; hold MODE for at least **1.2 seconds, then release**.
   Keep still for the following 2-second calibration. The screen must show a reference.
4. Tap GO. After 3 seconds, active timing begins. The sensor measures every 75ms.
5. The screen shows **Active** time and **In band** time. In band accumulates only
   during running, valid echoes within the default +/-4cm band, after a brief stable entry.
   Losing echoes or leaving the band stops that counter. This is a distance proxy,
   not a measurement of knee angle, full-body alignment or clinical correctness.
6. Tap GO to pause/resume. Hold GO for 1.2 seconds and release to finish.
   A SUMMARY line is emitted. Tap GO again to reset; the calibration is retained.

Commands: `wall`, `plank`, `cal`, `go`, `finish`, `reset`, `band 4`, `help`.
Mode, calibration and band changes require IDLE. Use `reset` from PAUSED/DONE as needed.

## A: wearable operation

One small board cannot monitor every body part at once. Use a secure removable
mount and **move/recalibrate the accelerometer for each exercise**. The R4 and
small breadboard should be fixed in a pouch; keep wiring clear of moving equipment.
Use tabletop/standing demonstration trials first, not a USB cable trailing on a treadmill.

| ID | Exercise | First-version behavior |
|---|---|---|
| 0 | Treadmill | Separate warm-up and 15-minute cardio timers; candidate HR if enabled |
| 1 | Thoracic spine rotations | Timer and manual reps; no measured vertical-axis rotation angle |
| 2 | Airplane | Timed hold and manual confirmation; no automatic balance/form grade |
| 3 | Chest press | Calibrated candidate reps; manual if motion signal is weak |
| 4 | Glute bridges | Calibrated candidate reps; manual if signal is weak |
| 5 | Lat pulldown | Calibrated candidate reps; manual if signal is weak |
| 6 | Farmer's carry | Duration and manually entered distance; no measured weight/distance |
| 7 | Hip flexor stretch | Hold timer and manual confirmation |
| 8 | Cat-cow | Calibrated candidate cycles; no spinal-curvature measurement |

1. While IDLE, type `select 5` for lat pulldown, or select another ID from the table.
   The optional knob works only when USE_SELECTOR_POT=1; it is off by default.
   When enabled, knob movement can change a profile previously chosen over Serial.
2. For a rep trial, attach MMA7361 to the forearm (press/pulldown), pelvis/torso
   (bridge), or upper back (cat-cow). Use the SAME placement for calibration and test.
3. Hold the wearable button >=1.2 seconds and release, or type `teach`. During the
   next 8 seconds perform about three comfortable full cycles. The code selects
   the analog axis with the largest excursion and learns the endpoints.
4. `teach_pass_candidate_only` means the signal span passed a simple signal check;
   it does not establish count accuracy. `teach_failed_use_manual` keeps manual mode.
   Never jerk or exaggerate the exercise to force a pass.
5. Tap the button (or `go`): 3-second countdown, then timing/counting. A full
   low-high-low signal crossing counts one candidate rep. There is no exercise-recognition AI.
6. Tap to pause/resume; hold and release to finish. A SUMMARY is emitted. Tap after
   DONE to reset. Reset preserves the current calibration until the profile changes.
7. For manual reps, type `manual` while IDLE, start the session, and type `rep` once
   per observed full repetition. Manual confirmations and automatic candidates are labelled.

The wearable's built-in LED flashes during teach/countdown and stays lit while RUNNING.
The OLED stays with station B. Without a laptop, A's profile selection/result detail
is not displayed, although its button and timer still run. Use USB logging for your demo.

### Treadmill timer and heart-rate demo

At ID 0 the default warm-up is **180 seconds** (an adjustable prototype setting),
then cardio is **900 seconds**. After the total active time, the session finishes.
Pauses are excluded. Change `warmupMs` if the assigned warm-up differs.

For a short presentation, type `demo` while IDLE: **5 seconds warm-up + 15 seconds
cardio**, explicitly labelled DEMO in the output. Type `real` while IDLE to restore
180+900 seconds. Demo output never represents a real completed 15-minute workout.

With verified HR hardware enabled, place a finger steadily on the optical window;
secure light, consistent contact, without constricting circulation. Type `baseline`
while IDLE to collect a 20-second candidate resting baseline. At least 10 one-second
usable estimates are needed. `goal 100` is an example of setting a goal supplied by
the instructor; it is not a recommended physiological target. `goal 0` disables it.
No age-based medical target is generated. The software reports NO_CONTACT,
ACQUIRING, MOTION or CANDIDATE; blank BPM means unavailable, not zero heart rate.
The motion/contact checks are heuristics and cannot prove a reading is accurate.
Validate at rest first; exercise readings remain an unvalidated demonstration.

Wearable commands: `help`, `select 0`..`select 8`, `teach`, `manual`, `go`, `rep`,
`finish`, `reset`, `distance 10` (metres, carry only), `baseline`, `goal 0`, `demo`, `real`.
Session reset/profile change/calibration are not persisted in EEPROM.

## Recording and validation

Serial Monitor shows logs but does not automatically save a file. For CSV files:

```text
python -m pip install pyserial
python tools/capture_serial.py --port COM4 --output station.csv
python tools/capture_serial.py --port COM5 --output wearable.csv
```

Use the actual COM ports, in separate terminals; close Serial Monitor before capturing
the same port. Type commands in the capture terminal. The logger adds host UTC time
and saves parsed columns using one common schema; empty fields are not applicable.
It refuses to overwrite an existing file. Press Ctrl+C to stop. COM port opening may
reset the Arduino, so start capturing BEFORE calibration. Device uptime clocks are
independent; use the host timestamps to compare the two files.

Do these checks before presenting:

| Check | Expected observation |
|---|---|
| Station, hand/cardboard target | Echo distance changes and a stable reference calibrates |
| Out of band / no echo | In-band total stops; active timer continues |
| Pause for 10 seconds | Neither active nor in-band total increases |
| Rep trial, manually count 10 reps | Compare exact totals; record errors over repeated trials |
| Pause halfway through a rep | Partial cycles across pause are not counted |
| Treadmill demo | WARMUP then CARDIO; finish after 20 active seconds |
| HR no contact / poor signal | Blank BPM and status; no invented sample values |
| Reset / profile change | No leftover session counts; recalibrate after moving sensor |

Treat a repetition profile as usable only after repeated comparisons with manual
counts. An initial engineering target is within +/-1 rep for at least 8 of 10 trials;
this is a proposed acceptance criterion, not a passed test or a clinical standard.

## Troubleshooting

- **OLED blank:** check printed GND/VCC/SCL/SDA order and module voltage rating.
  Run SerialDiagnostics after electrically checking the I2C device. Expect 0x3C or
  0x3D; change OLED_ADDRESS accordingly. Try the other OLED_SH1106 setting if the
  device responds but the image is missing/shifted. Scanner success does not prove
  electrical compatibility. The photographed YY module does not reveal its controller.
- **Station won't start:** calibrate while IDLE. Keep a broad reflecting target in
  the beam for the full calibration. Too close/far/noisy reference is rejected.
- **Counters unstable:** fix sensor mounting, reduce loose wiring, verify common
  ground and repeat calibration. Pure translation with little changing tilt may
  be hard for the accelerometer at comfortable exercise speeds; use manual mode.
- **Menu changes by itself:** With USE_SELECTOR_POT=1, check A3/wiper, shared
  ground and high-resistance B500K sampling behavior. For the first build keep
  USE_SELECTOR_POT=0 and use Serial commands. A disconnected A3 is harmless when
  the selector is disabled; its diagnostic readings will float.
- **Buttons always pressed:** two permanently connected switch legs were used.
  Use contacts that connect only on pressing; these are active-low bare switches.
  Grove's powered button module has different polarity and is not a drop-in replacement.
- **MAX30102 not found:** recheck the exact breakout's regulated supply, pull-up
  voltage and 5V-host interface. Do not move wires between 3.3V and 5V by trial and error.

## Evidence and limits

This is prototype code, not hardware-validated performance. Actual body placement,
echo behavior, count error, sensor contact and runtime must be tested on your build.
It neither measures force nor verifies full exercise technique. It does not control
the treadmill or change a participant's prescription. Use anonymous demonstration IDs/data.

Primary references used for the design:

- Course: uploaded `ECE 198 Project Parts List(1).pdf`; Group 23 pitch and exercise mapping.
- Arduino Uno R4 Minima datasheet: https://docs.arduino.cc/resources/datasheets/ABX00080-datasheet.pdf
- Uno R3 specs: https://store.arduino.cc/products/arduino-uno-rev3
- MMA7361L chip datasheet: https://www.nxp.com/docs/en/data-sheet/MMA7361L.pdf
- Matching-style MMA7361 breakout schematic: https://www.openimpulse.com/blog/wp-content/uploads/wpsc/downloadables/MMA7361-Schematic-Diagram.pdf
- Grove kit components: https://wiki.seeedstudio.com/Grove_Starter_Kit_Plus/
- HC-SR04 datasheet: https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf
- MAX30102 chip datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf
- SparkFun library: https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library
- U8x8 reference: https://github.com/olikraus/u8g2/wiki/u8x8reference
- Accelerometer inclination limits: https://www.analog.com/en/resources/app-notes/an-1057.html

The kit list describes parts supplied or borrowable, not confirmation that each
part is currently in hand. Firmware assumes R4 Minima + R3 per your pitch; your
later photographs established two breadboards and the OLED header, not Arduino model labels.

## Revision 2 - actual component photographs

Replaces the earlier assumed Grove sensor with the photographed MMA7361.
Adds fixed 3.3V control-pin connections, widens allowable taught signal span for
the more sensitive accelerometer, and disables the optional selector by default
because the photographed potentiometer is B500K. Station wiring is unchanged.
The photographed HC-SR04 matches the existing station design. These photographs
do not show the optical MAX30102 module, whose HR code remains disabled.
