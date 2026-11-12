# ECE 198 Group 23 - exercise monitoring prototype

Two Arduino programs with **one commented profile file for each of the 11 exercises**.
Use this repository as the shared source for the four-person build.

**Prototype status:** code can be compiled and logic-tested without hardware.
Sensor readings, repetition accuracy and placement must still be tested on the real build.

## Windows results app and B2.4

The desktop app shows measured hold statistics, explains Good / Not good yet /
Cannot evaluate, and keeps participant profiles and progression locally.
Download ECE198-Results-Windows from a successful **Build Windows results app**
run under Actions, extract it, and double-click ECE198_Results.exe.
No Python/PowerShell is needed for the executable.

- Import a CSV or read the connected Arduino (finish/save and close Serial Monitor first).
- Choose a participant label and select only their sessions when importing.
- Enter target durations and the required consistency. Review and confirm all rules
  before the app evaluates anything. The editable 90% default is a prototype setting.
- Good requires both reaching the entered in-band duration target and meeting the
  confirmed consistency requirement. No medical targets or weighted total are imposed.
- Corrected consistency = in-band time / (active time - final departure time) x 100.
  Earlier excursions followed by returning in band still count against consistency.
  Pauses are excluded. The final departure includes missing final readings; this
  defines an observed hold endpoint, not an inferred clinical failure or success.
- In-band time can be reduced by signal loss or delayed sampling. Uncredited time
  is not automatically labeled bad posture. No valid credited hold is unscorable.
- The app compares the same participant/exercise/tolerance under the current confirmed
  criteria. It shows hold time, completion, corrected consistency and change from the
  previous and first compatible attempts, plus a progression graph.
- SQLite history stays in %LOCALAPPDATA%/ECE198_Results/results.sqlite3. Duplicate
  imports are skipped; clearing the Arduino does not erase the computer's history.
  Import time is not exercise date. Keep participant assignment and mounting consistent.
- Export report opens a file chooser, so results can be saved in your chosen folder.

Upload B2.4's complete HoldStation folder to capture the final continuous departure
in future tests. Its base EEPROM records remain compatible with B2.3 summaries.
Those older summaries are preserved, but their missing ending interval is unknown;
the app shows their raw time and declines to invent a corrected consistency score.
The app's statistics assess measured proximity to a reference, not full-body form.

Source/build instructions are in [desktop/README.txt](desktop/README.txt).

## Easier updates (Windows)

Clone this repository once using **GitHub Desktop**, then use **Fetch origin /
Pull origin** for later updates. Open the sketch from the cloned folder and click
Upload in Arduino IDE. See [the step-by-step Git workflow](docs/GIT_WORKFLOW.md).
Fetching code alone does not change the program running on the Uno.

## Verify the station version: B2.4

The OLED title must show **WALL SIT B2.4** or **PLANK B2.4**.
Serial command `version` prints `# ECE198_B_2.4`.
Download/extract the current repository and open the new
`HoldStation/HoldStation.ino`, keeping its entire folder intact.
Uploading a previously opened sketch from an older folder does not update it.

For a bench check, keep a book still about 50 cm from the ultrasonic sensor.
Hold/release MODE to calibrate, then briefly tap/release GO.
After COUNTDOWN, RUNNING should show Band (in-band time) increasing in tenths of a second.
Hold GO >=1.2 seconds and release to save and show DONE.
One short GO from DONE resets both timers and starts a fresh countdown.
A long GO in IDLE/COUNTDOWN/DONE cannot create/finish a session.
At DONE, the OLED writes a fixed summary once: final times, save success/failure,\nreference, and next actions. Live distance and inside/outside changes no longer\nrewrite the completed screen. It stays visible until a new attempt/mode/calibration.
Serial button events `go_short` and `go_long` identify what the hardware detected.

If B2.3 shows RUNNING with WITHIN BAND but its counter stays at zero,
capture several DATA rows plus button events; the sample_gap_ms field helps
distinguish intermittent samples from a session-control problem.

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
before setting `ENABLE_MAX30102` to `1`; complete the electrical checks first.

References: [Arduino library installation](https://support.arduino.cc/hc/en-us/articles/5145457742236-Install-libraries-in-the-Arduino-IDE)
and [U8g2 source](https://github.com/olikraus/u8g2).

## Erase saved history using the buttons (B2.3)

1. Finish the current session, or stay in IDLE.
2. Hold **GO and MODE together for at least three seconds**.
3. The OLED shows **ERASE HISTORY?**. Release both buttons.
4. **Tap GO** to erase all saved summaries, or **tap MODE** to cancel.
5. Wait for **HISTORY CLEARED** before disconnecting power.

The opening hold and its release cannot confirm deletion. A short two-button hold
is ignored. The erase menu is unavailable during RUNNING, PAUSED, COUNTDOWN or
calibration. Erasing resets the session to IDLE while retaining calibration and
tolerance. CSV exports on your computer are untouched. The archive's EEPROM bytes
0..739 are cleared; session IDs restart at 1 and the archive generation increases. Export files from before/after an erase
are separate histories, so IDs alone must not be treated as globally unique.
Erasing is not atomic across a power cut; keep power connected until confirmation.
Use the next ordinary GO/MODE action to leave the confirmation/result display.

## Station session saving and restart

The station automatically saves a summary when you finish a RUNNING/PAUSED
session by holding GO >=1.2 seconds and releasing it, or sending `finish`.
Send `history` in Serial Monitor at 115200 baud with Newline to read saved results.
The latest 20 summaries survive power cycles in the Uno R3 EEPROM; new saves
replace the oldest. Running sessions are not recovered after power loss. Each
summary stores session ID, exercise, active/in-band milliseconds, reference,
band tolerance and (B2.4 onward) final-departure metadata. No calendar date is available without a real-time clock.

One short GO from DONE clears session timers and starts another three-second
countdown, retaining calibration. Long GO while IDLE/COUNTDOWN/DONE is ignored.
MODE from DONE lets you choose/calibrate another exercise. Both station exercises
now default to +/-10 cm. For accurate results, recalibrate after moving the sensor.

In-band timing uses consecutive valid samples, with each credited interval capped
at 150 ms; it no longer restarts a 300 ms settling period after delayed samples.
The first qualifying sample earns no time. Invalid/outside readings or pauses stop
credit immediately. See [the exercise guide](docs/EXERCISES.md) for details.
Install the complete updated HoldStation folder, including SavedSessions.h and
HoldTiming.h. No additional Arduino library is needed beyond U8g2; EEPROM is built in.

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


### Export saved station summaries to Excel/CSV

Finish and save the current session before connecting the exporter. Close Arduino
Serial Monitor. Opening the Uno serial port can reset the board (calibration is
cleared), but completed EEPROM summaries remain. No firmware update is needed if
the board already supports the `history` command.

From the repository folder:

```sh
python -m pip install -r requirements.txt
python tools/export_sessions.py --port COM4 --output saved_sessions.csv
```

Replace COM4 with the station's actual port. With no `--output`, the script chooses
a timestamped filename. It reads the latest 20 summaries and exits. Open the CSV
in Excel later. Each row includes session ID, exercise, active/in-band time in
milliseconds and seconds, reference distance and tolerance. `exported_at_utc`
is the time of export, not the session date; the Arduino has no real-time clock.

Exporting does not erase the board's history. Existing files are never overwritten.
Export before more than 20 newer sessions replace results you want to keep.
Snapshots from separate exports can overlap; use session_id to identify duplicates
from the same board/history. To record every future live session while connected,
use `tools/capture_serial.py` instead.

Exporter tests:
```sh
python -m unittest discover -s tests -p test_export_sessions.py
```

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


### Readable reports and participant management

The Windows results app has 7/10 visible attempt rows, specific time/consistency
reasons, a compact selected-attempt summary, and **Clean report (PDF)** with graphs
and progression. **Export CSV** remains available. **Manage participants** searches
labels and deletes an entire local profile (criteria and attempts) after confirmation;
it does not alter exported files or Arduino records. This update needs no Arduino
upload. See `desktop/README.txt` for the app instructions.


### B2.5 chest press (same Solution B wiring)

MODE now cycles wall sit, plank and chest press. Chest press uses two stable
reference positions: hold MODE to capture the start, then repeat at full press.
Good reps reach 80% of that calibrated travel and return within 20% of the start.
GO retains its start/pause/resume/long-finish controls. The OLED displays good and
completed-attempt counts. Saved chest sessions occupy a separate ten-record EEPROM
archive, preserving all twenty hold slots. The existing app continues to import
wall-sit/plank only; chest-press app integration and the blinking LED are deferred.

See [calibration, controls and physical tests](docs/CHEST_PRESS.md). This update
requires uploading `HoldStation/HoldStation.ino`; it does not require a new PC app.


### B2.6: discard an attempt without saving

In any exercise, short GO pauses; hold MODE for **2 seconds** and release to open
CANCEL SESSION? Short GO confirms discard; short MODE returns to PAUSED. Saved
history and calibration are kept. The OLED confirms CANCELLED / NOT SAVED.
Chest's title is shortened to CHEST B2.6. The app and LED remain unchanged.
