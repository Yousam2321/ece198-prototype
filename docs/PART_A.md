# Part A A2.0 — build, controls and testing

This version replaces the A1.x control/session implementation with explicit states, following the approach used by Part B. It targets the **Uno R4 Minima**. The team's squat/lateral-raise criteria, pin mapping, saved-record format and app compatibility are retained. No physical board is connected to the development environment; the tests below must still be run on the prototype.

## Install and upload

Install **LiquidCrystal by Arduino** and **SparkFun MAX3010x Pulse and Proximity Sensor Library** through Arduino IDE's Library Manager. MAX30105.h comes from that SparkFun library and is also used for MAX30102.

Keep all four files together: PartA.ino, PartACore.h, PartASerial.h and PartASessions.h. Open PartA.ino, choose Arduino Uno R4 Minima and the board's port (**COM6** for the current prototype), and upload. Keep the accelerometer still during the roughly one-second startup calibration. The LCD then shows **SQ A2.0 READY** or **LAT A2.0 READY**.

## Existing wiring

| Component | Connection |
|---|---|
| START / GO button | D2 to GND when pressed; internal pull-up |
| EXERCISE button | D3 to GND when pressed; internal pull-up |
| MMA7361 X / Y / Z | A0 / A1 / A2 |
| Parallel LCD RS / Enable | D7 / D8 |
| Parallel LCD D4 / D5 / D6 / D7 | D9 / D10 / D11 / D12 |
| MAX30102 data | Board-labelled SDA / SCL |

Keep the previously working sensor power wiring and common ground. This update adds no shield, LED or extra component. SQ means squats; LAT means lateral raises.

## Button controls

| State | START / GO | EXERCISE |
|---|---|---|
| READY | Tap/release starts a 3-second countdown | Tap/release changes exercise |
| COUNTDOWN | Tap/release cancels the countdown | Does not change exercise |
| RUNNING | Tap/release pauses; hold 3 seconds finishes/saves | Does not change exercise |
| PAUSED | Tap/release resumes; hold 3 seconds finishes/saves | Hold 2 seconds opens Cancel attempt |
| DONE / SAVED | One tap starts a fresh countdown/attempt | One tap changes exercise and returns READY |

A long START fires once while held. Releasing it cannot start or pause another session. The saved result stays on screen, with final reps and active seconds, until a deliberate next action. Repeating a long START at DONE cannot duplicate a record.

To stop without saving: pause, hold EXERCISE 2 seconds, release, then choose GO=yes or EX=no. Cancelling the pending attempt leaves earlier saved history intact. A paused attempt no longer saves automatically when EXERCISE is pressed; switching is allowed only at READY/DONE, after explicitly saving or cancelling.

For exact stop timing, tap START to pause at the end of the exercise, then hold START 3 seconds to save. If held while still running, active time includes the hold until saving begins.

## History erase at any session state

Hold both buttons together for 3 seconds until **ERASE HISTORY?** appears. Release both. A fresh START confirms; EXERCISE cancels. The opening hold cannot confirm deletion.

The menu pauses the current attempt. After two seconds of result feedback it returns to the previous state, excluding menu time; countdowns retain their remaining time. Current reps, active time and accepted-beat statistics stay in RAM. An unfinished movement must restart after the pause. Only the completed board archive is cleared. Exported CSVs and app participants/attempts are separate.

## Detector criteria retained from the team sketch

Accelerometer baseline calibration averages 200 readings, taken 5 ms apart. After calibration, readings update every 25 ms and the baseline uses an EMA with alpha 0.05. Thresholds are relative ADC readings, not joint angles.

| Exercise | First phase | Return phase | Entry cooldown |
|---|---|---|---|
| Squat | Z ≤ -40 for 3 consecutive readings | Z ≥ -5 for 8 consecutive readings | 1200 ms |
| Lateral raise | Y ≥ **25** for 3 consecutive readings | Y ≤ **5** for 8 consecutive readings | 1200 ms |

A completed return adds one rep. Pauses, menus and sampling gaps over 150 ms reset incomplete movements. Counters saturate at 32767 rather than wrapping. These records contain rep counts, not failed-attempt counts or a movement-quality percentage.

## Pulse and control responsiveness

Pulse data remains enabled. The same IR presence threshold (50000), accepted BPM interval (40–180) and rolling five-beat LCD mean are used. Saved mean BPM averages accepted active-session beats only. No accepted beats produces an empty CSV field, displayed as unavailable in the app.

The main loop does not call getIR()/safeCheck(250). It polls buffered samples instead, bounds Wire transfers to 5 ms, and disables pulse polling after a failed health probe so a missing pulse sensor does not stop the controls. Startup sensor configuration is separate from the button/session loop.

## Save, read and export

Finish/save explicitly. A successful save enters DONE and retains the result. A failed verification enters PAUSED with **SAVE FAILED** and keeps reps/time for a long-START retry. The latest **20 combined squat/lateral-raise records** persist in EEPROM. The existing A1.x record layout, CRC, generation journal and overwrite order are unchanged.

Normal use produces **no continuous Serial output**. It does not require an open Serial Monitor. At 115200 baud with Newline, the supported commands are:

| Command | Result |
|---|---|
| version | # DEVICE,part_a,A2.0 |
| status | Exercise, state, active milliseconds, reps, pulse-ready flag, START-pressed flag, EXERCISE-pressed flag |
| history | Both exercises, using the existing ASAVED format |
| erase CONFIRM | Clears completed history while not running/counting down |

History output is streamed in small chunks between button/sensor/display updates. A full or closed COM connection cannot turn the exporter into a wait loop. Running/countdown sessions must be paused or finished before history is read. A partial export never receives a success terminator. Opening a connection can reset a board, so finish/save before importing.

From the project root in PowerShell, with Serial Monitor closed:

~~~powershell
python tools/export_sessions.py --port COM6 --part-a --output ".\saved_sessions\saved_sessions_$(Get-Date -Format yyyyMMdd_HHmmss).csv"
~~~

The current Windows app's **Read COM port** and CSV import already accept this schema; no executable update is required for A2.0. Export does not erase saved rows. Saved sessions have archive IDs, not calendar dates.

## Physical acceptance test

1. With Serial Monitor closed, wait for READY, tap START, wait for RUNNING, and do two movements. Pause; confirm reps and time stop changing.
2. Hold START 3 seconds. Confirm SAVED shows the final reps/time and remains stable.
3. Tap START once. Confirm the fresh countdown starts, reps/time reset, and a second session can be saved. Repeat at least five times, including switching exercises after DONE.
4. Export from COM6, then immediately start/pause/resume/save another attempt without unplugging/resetting. Export again and check the new row.
5. Test pause → Cancel attempt, first choosing EX/no and then GO/yes. No discarded attempt should be saved.
6. Open the both-button erase menu at READY, RUNNING, PAUSED and DONE. Cancel first, then confirm. Check saved history clears, current pending data stays, and the correct state returns.
7. Power-cycle after saving and read history again. Do not use an unsaved attempt for the retention test.

If a control is unresponsive, record the exact LCD state and send version and status through Serial Monitor. Status includes the two raw button flags to distinguish input from state-transition problems.
