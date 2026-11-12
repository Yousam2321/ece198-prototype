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
