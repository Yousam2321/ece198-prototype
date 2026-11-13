# Validation — Part A A2.0 / Part B B2.7

No physical board is connected to this environment. Build and simulation results do not prove the reported hardware freeze is resolved, or establish motion/pulse accuracy. The user withdrew the earlier Serial Monitor correlation; it is not treated as an established cause.

## Part A scope

The A1.x session/button implementation is replaced by an explicit state machine following Part B's approach. A2.0 targets the Uno R4 Minima; selecting an AVR board for Part A produces a clear compilation error. Part B continues to target the Uno R3.

Retained: sensor pins, MMA7361 baseline averaging/EMA, squat -40/-5 and lateral-raise 25/5 thresholds, 3/8 confirmation samples, 1200 ms movement-entry cooldown, pulse presence/accepted-beat limits, ASAVED protocol, the latest 20 combined records and CRC/generation/EEPROM layout. PartASessions.h is unchanged.

Changed: START begins a three-second countdown; DONE retains results and starts the next attempt in one tap; mode switching is limited to READY/DONE after explicit save/cancel; long START saves once; paused long EXERCISE opens cancel confirmation; both-button erase is available in every session state. Menus preserve pending attempts and exclude their time. Incomplete detector phases reset on pause/menu/sample gap.

Responsiveness changes: no continuous serial telemetry or button-event printing, no getIR()/safeCheck(250) wait, bounded Wire transfers and a pulse health probe, incremental command/history output with no waiting for host output space. Connection loss aborts pending output without changing session state. LCD lines are padded and updated only when their contents change.

## Automated checks

- tests/run_tests.py executes the actual A2 sketch under generic and native-R4 serial backends. The native fake rejects core serial writer/boolean calls.
- Standalone operation with no monitor: startup calibration, countdown, reps, pause/resume, saving, stable DONE screen, no duplicate saves and one-tap fresh attempts.
- Original detector criteria: full squat/raise, shallow lateral excursion rejected at 25, brief spike rejected, and no accidental mode switch during active/paused attempts.
- Save verification failure keeps counts/time paused; retry writes one completed record. A repeated finish at DONE cannot duplicate it.
- Erase/cancel menus: release-before-choice, cancelling preserves saved bytes, running/paused/DONE state restoration, pending reps/time retained, menu time excluded, countdown time preserved, and cancelling a pending attempt creates no record.
- Pulse polling never calls the fake blocking getIR method; accepted active beats save correctly, and a failed pulse health check leaves session controls working.
- Closed and stalled COM: session actions and saves continue; five close/reopen/export cycles complete without a simulated reset. Native close/full-buffer submission races return with USB IRQs restored.
- The actual emitted ASAVED rows pass through the Python CSV exporter and the desktop CSV reader. No app schema/scoring change is needed.
- Existing archive ring/CRC/generation behavior, readback through a fresh archive instance and interrupted save/erase checks are retained.
- Unsigned session clocks are checked across millis() rollover.
- All 46 Python evaluator/parser/store/report tests pass against the current app source, including the separate report simplification already in the repository.
- Part B timing, OLED, distance/hold restart and chest-press regressions remain passing. Part B source is not changed by this rewrite.

The local R4 build uses approximately 67 kB flash and 6.3 kB static RAM; CI performs the final R4 build plus Part B SH1106, SSD1306 and no-OLED builds. Renesas core 1.6.0, AVR core 1.8.8, LiquidCrystal 1.0.7, SparkFun MAX3010x 1.1.2 and U8g2 are used in CI.

## User-side acceptance still required

Upload the entire PartA folder, keep the accelerometer still through calibration and verify the A2.0 READY screen. Start/count/pause/save at least five attempts, including both exercises. Each SAVED screen must retain the final result; one START tap must begin a fresh countdown. Export, immediately use the buttons without reset/unplug, save again and verify a second export contains the new record. Check cancel and erase/no-erase choices, then power-cycle after saving and verify retention.

For an unresponsive board, capture the displayed state plus version/status output. The status command includes raw START/EXERCISE flags so input failures can be distinguished from controller transitions. Physical sensor placement, real rep accuracy, BPM/contact behavior and post-save/export responsiveness must be checked on the prototype.
