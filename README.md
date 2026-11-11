# ECE 198 Group 23 - exercise monitoring prototype

Two Arduino programs with **one commented profile file for each of the 11 exercises**.
Use this repository as the shared source for the four-person build.

**Prototype status:** code can be compiled and logic-tested without hardware.
Sensor readings, repetition accuracy and placement must still be tested on the real build.

## Start here

1. Open [the team build/test plan](docs/Team_Build_and_Test_Plan.pdf): both schematics and four task pages.
2. Read [the exercise guide](docs/EXERCISES.md) for exact commands and implemented behavior.
3. Open the matching sketch in Arduino IDE:
   - `WearableMonitor/WearableMonitor.ino` for the **Uno R4 Minima**.
   - `HoldStation/HoldStation.ino` for the **Uno R3**.
4. Select the correct board and USB port. Install the libraries below.
5. Open the `Config.h` tab for that sketch. Settings moved here from the older sketch headers.
6. Upload; open Serial Monitor at **115200 baud**, **Newline**; send `help`.

Keep each sketch folder intact. Upload one main program per Arduino; you do not
upload every exercise file separately. Each Arduino selects the relevant exercise at runtime.

## Files the team edits

| Owner | Primary files | Work |
|---|---|---|
| Person 1 | `docs/Team_Build_and_Test_Plan.pdf` | Station wiring, mounting and electrical checks |
| Person 2 | `HoldStation/` | Wall-sit/plank logic, OLED and distance tests |
| Person 3 | `docs/HARDWARE.md` | Wearable wiring, MMA7361 and GY-MAX30102 interface checks |
| Person 4 | `WearableMonitor/` | Exercise profiles, timers, candidate reps and optional HR |

A full pin/reference guide is in [BUILD_DETAILS.md](docs/BUILD_DETAILS.md).
The B500K potentiometer and spare seven-segment display are omitted from the first build.
No Grove shield is needed. No wireless link between the two units is implemented.

## Configuration and libraries

| Program | Settings | Arduino Library Manager |
|---|---|---|
| Wearable | `WearableMonitor/Config.h`: HR off by default; optional pot off | SparkFun MAX3010x Pulse and Proximity Sensor Library **only when HR is enabled** |
| Station | `HoldStation/Config.h`: OLED on; SH1106 initially; address 0x3C | U8g2 by oliver |

Use Arduino AVR Boards for Uno R3 and Arduino UNO R4 Boards for R4 Minima.
Tested core/library versions and build results are in [VALIDATION.md](VALIDATION.md).
Before enabling HR, complete the GY board interface checks in the team plan. The front/back
photographs identify the five-pin GY-MAX30102; they do not replace voltage measurements.
No SpO2 calculation is implemented. Medical exercise targets are not generated.

## Arduino IDE setup and missing-library errors

Install libraries on each teammate's computer; downloading this repository does not install them.

For the station OLED:
1. In Arduino IDE, open **Sketch > Include Library > Manage Libraries** (or press **Ctrl+Shift+I** on Windows).
2. Search for **U8g2**.
3. Install **U8g2 by oliver**. Version **2.37.1** was used for the recorded builds.
4. Click **Verify** again.

`U8x8lib.h` is a header supplied by U8g2. If compilation says
`U8x8lib.h: No such file or directory`, the IDE cannot find that library.
Keep `#include <U8x8lib.h>` in the sketch and install the full library.
If it is already marked Installed, restart the IDE and verify again.

To test the station through Serial without the OLED library, change
`#define ENABLE_OLED 1` to `#define ENABLE_OLED 0` in
`HoldStation/Config.h`. The display is disabled in this mode.

For the wearable, install **SparkFun MAX3010x Pulse and Proximity Sensor Library**
before setting `ENABLE_HEART_RATE` to `1`; complete the electrical checks first.

References: [Arduino library installation](https://support.arduino.cc/hc/en-us/articles/5145457742236-Install-libraries-in-the-Arduino-IDE)
and [U8g2 source](https://github.com/olikraus/u8g2).

## Save data

Serial Monitor does not save a file automatically. Close it before using the same port:

```sh
python -m pip install -r requirements.txt
python tools/capture_serial.py --port COM4 --output station.csv
```

Use your actual port. Use another terminal and filename for the wearable.
The script accepts commands, adds UTC timestamps and refuses to overwrite files.
Start capture before calibration because opening a serial port may reset the board.
Logs are excluded from Git by default. Use anonymous trial IDs.

## Tests

```sh
python tests/run_tests.py
```

This compiles/runs portable C++ logic tests (needs `g++`) and checks that the two
copies of `PrototypeCore.h` agree. Actual board compilation and hardware tests are separate.
Do not describe a passing build as proof of exercise measurement accuracy.

## Team Git workflow

The team repository is **https://github.com/Yousam2321/ece198-prototype**.
See [GIT_WORKFLOW.md](docs/GIT_WORKFLOW.md) for cloning, branches and reviews.

No open-source license has been chosen on the team's behalf. Third-party libraries are
installed separately through Arduino IDE and retain their own licenses.
