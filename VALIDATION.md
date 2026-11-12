# Validation - commented Git repository

All seven board configurations compiled after the exercise profiles were separated.
The physical hardware was not connected to this environment. No claim of measured
rep-count or heart-rate accuracy follows from these builds.

| Build | Board | Flash bytes used | Static RAM bytes used | Result |
|---|---|---|---|---|
| repo_wearable | arduino:renesas_uno:minima | 53076 | 4624 | PASS |
| repo_hr | arduino:renesas_uno:minima | 61340 | 5968 | PASS |
| repo_pot | arduino:renesas_uno:minima | 53240 | 4636 | PASS |
| repo_hr_pot | arduino:renesas_uno:minima | 61504 | 5980 | PASS |
| repo_station | arduino:avr:uno | 16692 | 1422 | PASS |
| repo_ssd | arduino:avr:uno | 16692 | 1422 | PASS |
| repo_serial | arduino:avr:uno | 9376 | 833 | PASS |

- Board cores: Arduino AVR 1.8.8; Renesas Uno 1.6.0.
- Libraries: U8g2 2.37.1; SparkFun MAX3010x 1.1.2.
- Portable tests passed for countdown, pause/resume, clock wrap, full/partial/fast/
  expired cycles, range dropouts, long sample gaps and button debounce.
- Two copies of PrototypeCore.h match; capture_serial.py and run_tests.py pass Python syntax checks.
- Upstream board-core sources emit warnings; no project-source warning was reported.
- The R3 OLED build leaves 626 bytes beyond static allocations for stack/runtime data.
  U8x8 avoids a full-screen buffer; the code avoids String objects and recursion.

Hardware checks remain: wiring/supply/interface, OLED driver, body placement,
repeat-count error, optical contact/motion artifacts, hold geometry and timing.
Use the four task pages in docs/Team_Build_and_Test_Plan.pdf to record real results.

The shared core was given extra explanatory comments after compilation. Its code
was unchanged; the portable test runner was rerun successfully on those final files.

## OLED refresh fix — November 11, 2026

The station now pads each row to 16 characters and only writes rows whose text
changes. It no longer clears each line before rewriting it every 250 ms.
The default Uno R3 / SH1106 configuration compiled successfully with the same
installed core and U8g2 library: 16,704 bytes flash, 1,558 bytes static RAM.
The row cache costs 136 bytes of static RAM; 490 bytes remain for stack/runtime
allocations. The compiler reports low available memory, so runtime stability
still needs hardware testing. Distance acquisition and calibration are unchanged.
The build used installed dependencies despite a failed network index refresh.

## Station persistence, restart and in-band timing update

- Default Uno R3/SH1106 build passed: 18,620 bytes flash; 1,265 bytes static RAM,
  leaving 783 bytes for stack/runtime data. Event strings moved to flash to reduce RAM.
- EEPROM archive uses addresses 0..419 for 20 records; version, CRC and a commit
  marker reject incomplete/corrupt entries. No continuous EEPROM writes occur.
- Host tests execute the actual station sketch with simulated Arduino/EEPROM APIs:
  idle/countdown long-press finish rejection, duplicate-save prevention, one-press
  restart with fresh timers, pause/resume and summary contents.
- Archive tests cover reboot reads, ring overwrite, corruption and interruption at
  every write boundary. Timing tests cover delayed samples, outside/invalid input,
  pause, reset and millis rollover. Existing shared-core tests also passed.
- Physical OLED/button/range/EEPROM behavior still needs testing on the user's board.
- The wearable still uses its unchanged shared RangeTimer; station now uses HoldTiming.

## B2.1 version and end-to-end regression checks

- OLED title and Serial version command identify B2.1, preventing ambiguity
  between a downloaded folder and the sketch actually running on the Uno.
- In-band display uses tenths of a second; GO release events identify short/long.
- Fresh calibration is checked after a DONE reset before starting again.
- The host integration test now drives the actual firmware loop with simulated
  HC-SR04 echoes and timed button presses. It calibrates, counts over five seconds
  in band, pauses/resumes, saves, and repeats a complete second session.
  It verifies long idle/done presses cannot create empty or duplicate summaries.
- Uno R3 / default SH1106 build: 18,780 bytes flash; 1,287 bytes static RAM,
  leaving 761 bytes for stack/runtime. Existing portable tests pass.
- No physical board is attached here; actual wiring, input signals and timings
  still need the bench check documented in README.

## B2.2 completed OLED screen

- DONE renders final times/reference and EEPROM save result once. It no longer
  renders live range or inside/outside changes; a new session clears the latch.
- The compact Band label retains tenths without overflowing the 16-character row.
- OLED-enabled host integration tests verify that sensor changes after DONE cause
  zero additional OLED writes and that restarting updates the display again.
  The save-failure display state is checked separately.
- Shared-core, serial-only and OLED-stub station tests passed. Default Uno
  R3/SH1106 firmware compiled: 18,912 bytes flash; 1,327 bytes static RAM;
  721 bytes left for stack/runtime. Physical flicker still needs a board check.
- GitHub Desktop clone/fetch/pull instructions added to avoid repeated ZIP downloads.

## B2.3 button-only erase

- Both buttons held >=3s in IDLE/DONE open an erase confirmation. Both must be
  released before a separate short GO can confirm; a short MODE cancels.
- Tests cover aborted holds, cancel without mutation, release without confirmation,
  confirmed clear from IDLE and DONE, persistence after controller recreation,
  IDs restarting at 1, and no erase/finish/calibration from a two-button running hold.
- OLED tests cover erase prompt and cleared feedback. Archive tests verify bytes
  0..419 are zeroed and unrelated EEPROM addresses remain untouched.
- Shared-core, serial-only and OLED-enabled station tests passed. Default Uno R3
  build: 19,670 bytes flash; 1,456 bytes static RAM; 592 bytes left for runtime/stack.
- Physical button/UI and EEPROM erase behavior still need testing on the real board.

## B2.4 and Windows results app

- Firmware saves final-departure active duration, whether any valid hold was seen,
  and an archive generation. Pauses do not extend the recorded departure.
- Base EEPROM summaries at 0..419 remain readable; new metadata at 420..739 is
  CRC-bound to its base record. Archive generation uses alternating CRC records at
  900..915 and increments on clear. Old summaries are preserved with unknown metadata.
- Corrupt/missing extension data never becomes an invented zero ending duration.
- Tests cover final departure, a middle departure followed by return, pause clocks,
  zero holds, legacy records, corrupted extensions, interrupted writes at every
  append boundary, and identities across erase/reboot.
- Python tests cover exact score boundaries, time/consistency feedback, no invented
  legacy score, SQLite reopen/deduplication/profile separation, archive generations,
  and same-exercise/tolerance comparisons.
- R3 SH1106 build: 21,384 bytes flash; 1,461 bytes static RAM; 587 bytes for stack/runtime.
- Windows CI builds the executable and smoke-tests both source GUI and packaged EXE.
  A synthetic-results GUI screenshot is produced for visual review. The GUI cannot
  run in the local headless Linux environment, so Windows is the GUI test environment.
- The current Python suite has 18 passing tests, plus the portable C++ station/core
  tests in serial and OLED-stub configurations. Physical B2.4 testing remains required.
