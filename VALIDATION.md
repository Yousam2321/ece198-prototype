# Validation — Part A A1.0 / Part B B2.6

No physical board is connected to this environment. Builds and simulated inputs do not establish real rep, range or pulse accuracy.

## Part A integration

The uploaded working `ECE198_two_working_exercises.ino` was committed as the Part A baseline before integration. Function-body comparisons verify that `updateSquatCounter`, `updateLatRaiseCounter`, `updateAccelerometer` and `updateLCD` are identical to that upload. Existing pins, thresholds, confirmation sample counts, cooldowns and button timings remain. Additions surround start/pause/end/switch and accepted beats to record duration and save before reset.

| Local build | Core | Flash | Static RAM | Result |
|---|---|---:|---:|---|
| Part A, Uno R3 | Arduino AVR 1.8.8 | 18,004 / 32,256 bytes | 1,291 / 2,048 bytes | PASS |
| Part A, Uno R4 Minima | Renesas Uno 1.6.0 | 66,316 / 262,144 bytes | 5,992 / 32,768 bytes | PASS |

Libraries: LiquidCrystal 1.0.7, SparkFun MAX3010x 1.1.2. No full-screen buffer or dynamic String additions are used. R3 leaves 757 bytes after static allocations; runtime stack/physical stability still need bench testing.

Host tests execute the actual sketch loop with simulated ADC, buttons, clock, pulse and EEPROM. They cover both original rep detectors, running-switch restriction, pause/resume active timing, save-before-END/reset, save-before-switch, no duplicate save, unavailable BPM versus accepted BPM, power-cycle reads, twenty-record ring replacement, corruption rejection, interrupted appends and every erase write boundary. The erase generation hides old entries once committed.

## Exporter and desktop app

**43 Python tests pass locally**, covering existing hold/chest logic and new Part A protocol, fragment reassembly, explicit completion, rejection of partial/conflicting/invalid data, missing BPM, CSV writing, direct board auto-detection, count-only evaluation, target validation, existing profiles, duplicate import, exact-row deletion, progression and mixed/count-only PDF/CSV reports.

Part A is evaluated by recorded rep count only; no failed-attempt percentage is invented. Mean accepted BPM and active time are informational. Missing beats are not a zero BPM result. The app uses the same database location and schema, preserving pre-update profiles and B records. Existing goal settings load with blank new fields until the user adds A goals.

A synthetic eleven-page report exercises all five exercises, continuation pages, no BPM, zero reps, reason text and same-exercise graphs. Rendered-page checks are separate from numeric assertions. Report samples contain no participant data.

Windows CI builds the one-file executable and runs both source and packaged GUI smoke tests. These include opening a pre-update B profile and an empty profile, importing both A exercises, missing/present BPM, exact goal verdicts, the scrollable criteria dialog, graphs, PDF/CSV round trips, participant/row deletion and hold/chest regressions. Check the linked GitHub Actions run for its outcome. The local Linux environment is headless, so Windows is the executable/GUI test environment.

## Part B regression

Every existing `HoldStation/` file matches published main byte-for-byte. Portable core, actual station controller (OLED on/off), hold timing, cancel/DONE/restart/erase and chest-counter tests still pass. CI retains all three B Uno builds (SH1106, SSD1306 and Serial only). There are no new LED behaviours.

Previously reported user bench checks included stopwatch accuracy, pause/resume, in-band stopping, calibration, GO/MODE behaviour, completed-history power cycle, deletion cancellation and chest repetitions. This integration does not repeat or replace those physical results.

## Remaining physical checks

Use [docs/PART_A.md](docs/PART_A.md): compare reps with an observer, pause/resume timing, finish and mode-switch saves, optional BPM contact, completed-session power cycle, A/B app reading, CSV/PDF round trip and intentional history erase. Compare source and board versions/ports before uploading. Export any needed records before ring replacement or intentional erasure.


## Part A A1.1 — button-confirmed history erase

- START finish/save is now 3 seconds. EXERCISE switches on release, allowing the two-button chord to be consumed before a mode switch.
- Before starting or after finishing, START+EXERCISE held 3 seconds opens the LCD erase confirmation. Both must be released before a separate START confirm or EXERCISE cancel. Running/paused attempts cannot open the menu; their chords do not finish/save/pause/switch.
- Actual-sketch loop tests check subthreshold holds, either button pressed first, holding/releasing the opening chord, simultaneous confirmation inputs, cancellation with byte-identical EEPROM, successful clear and generation change, reboot reads, running/paused guards and normal subsequent finish/save. LCD assertions check the prompt and outcomes.
- Portable C++ core, Part B OLED on/off, chest and extended Part A tests pass. No physical board testing was performed.
- A1.1 local Uno R3 build: 19,110 bytes flash; 1,316 bytes static RAM; 732 bytes left after static allocation. R4 Minima: 67,276 bytes flash; 6,024 bytes static RAM. Both compile successfully with the previously recorded cores/libraries.
- Detector, accelerometer and normal LCD update function bodies remain unchanged. Existing EEPROM format, serial history/erase commands and desktop compatibility are preserved. Part B and the app's source logic are unchanged.


## Participant defaults and A1.2 serial recovery

- Every participant-name change and Open profile loads saved criteria with defaults for missing/empty goals. Startup/deletion use the same path. Defaults are editable 5-second holds, 5-rep goals and 90% requirements; saved custom values are preserved and default loading does not write settings or implicitly confirm them.
- 46 Python tests pass, including fresh/partial/empty profiles, saved custom criteria, non-mutation and fallback boundaries. Windows GUI checks also switch between new and existing labels, check defaults before Open profile, retain edited goals, require confirmation and preserve database history.
- Source inspection of Arduino Renesas core 1.6.0 SerialUSB.cpp found a bulk-write loop with no exit when CDC output space remains zero. PartASerial.h avoids this path using available-space checks and single-byte writes, with a 20 ms bounded wait. XYZ output is optional; sampling is not conditional on the connection. History transfers with a write error do not emit a completion marker. This addresses a code-level freeze risk; the reported board still needs a physical post-export check.
- Actual-sketch tests export via the command parser, close the simulated port, start/count/pause/resume/finish/save without reset, reopen and export again. A connected stalled port times out rather than entering the raw write; partial exports lack the completion marker. All prior B/controller/archive tests pass.
- A1.2 local builds pass: Uno R3 19,354 bytes flash / 1,335 static RAM; R4 Minima 67,476 bytes flash / 6,040 static RAM. EEPROM and CSV formats remain unchanged.


## Part A A1.3 — revised lateral-raise threshold

- Compared the newly uploaded ECE_198_with_lat_raises(1).ino against the earlier team sketch. Its only functional change is LAT_RAISE_UP_THRESHOLD from 20.0 to 25.0; return threshold, timing, detectors, pins and LCD are otherwise the same.
- Merge that value into A1.2 and update the device version to A1.3. Keep existing save-before-reset/switch, 3-second finish, button-confirmed erase, CRC history, structured export and bounded serial output. Part B and the app are unchanged.
- The actual-sketch ADC test now includes a shallow Y excursion that triggered the old threshold but does not yield three qualifying samples with the revised threshold and baseline tracking. It also checks a brief spike is ignored and a complete raise/return still counts and saves. Existing controller/archive/serial recovery regressions pass.
- Both local builds pass with unchanged usage: Uno R3 19,354 bytes flash / 1,335 bytes static RAM; R4 Minima 67,476 bytes flash / 6,040 bytes static RAM.
- Physical rep accuracy and reduction in false counts remain user-side tests with consistent mounting. No new app upload or EEPROM erase is required.


## Part A A1.4 — history erase in every session state

- Remove the beginning/finished-only button-menu gate. Capture the previous running state and pause the current attempt as soon as a two-button chord starts; preserve reps, active time, touched state and accepted-beat statistics. Exclude menu/result time, reset incomplete detector phases and restore the previous running/paused state when leaving. Short aborted holds also return without saving or clearing history.
- Actual-sketch tests cover menu cancel during a partial lateral raise without crediting it across the pause; early-aborted holds while running; running cancel with byte-identical EEPROM; running clear with retained count and resumed state; paused cancel/clear with unchanged active time/count; and subsequently saving the preserved attempt under the new archive generation. Existing idle/finished confirmation, accidental-confirmation, persistence, serial close/stall and B regressions pass.
- Both local builds pass: Uno R3 19,370 bytes flash / 1,335 bytes static RAM; R4 Minima 67,492 bytes flash / 6,048 bytes static RAM. Archive/CSV formats and app compatibility remain unchanged.
- This supersedes A1.1's restriction on opening the menu while running/paused. Only saved history is erased; finishing the still-current attempt later can add a new saved row. Physical testing remains the user's check.
