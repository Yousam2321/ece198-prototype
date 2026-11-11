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
