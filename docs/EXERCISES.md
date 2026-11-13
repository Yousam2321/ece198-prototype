# Implemented exercises and controls

| Exercise | Board | Measured result |
|---|---|---|
| Squats | Part A A1.2 | Original accelerometer rep count, active time, optional mean accepted BPM |
| Lateral raises | Part A A1.2 | Original accelerometer rep count, active time, optional mean accepted BPM |
| Wall sit | Part B B2.7 | Active/in-band time; final continuous departure for corrected consistency |
| Plank | Part B B2.7 | Active/in-band time; final continuous departure for corrected consistency |
| Chest press | Part B B2.7 | Completed attempts, good reps, active time and calibrated travel |

The previous experimental WearableMonitor and unimplemented exercise profiles have been removed. Part A uses the team's working two-exercise sketch. See [PART_A.md](PART_A.md) for thresholds and integration details.

## Part A

Short START release starts, pauses or resumes. Long START 3 seconds ends/saves, then clears counters as before. EXERCISE selects Squats / Lat Raises only while stopped; a started paused session saves before switching. Twenty completed sessions persist in EEPROM. Serial `history` exports both exercises; `erase CONFIRM` clears saved A history while stopped. The app judges the rep-count goal only; there is no failed-attempt denominator.

To erase A history by buttons, finish/save first, hold START+EXERCISE for 3 seconds and release both. At the confirmation prompt, tap/release START to erase or EXERCISE to cancel. Running/paused sessions cannot enter this menu.

## Part B wall sit / plank

Select the exercise with a short MODE while stopped. Put the target/body in the chosen reference position and hold MODE (at least 1.2 s, then release) to calibrate; keep it steady during collection. Short GO starts the countdown, then RUNNING. GO short pauses/resumes. The band defaults to ±10 cm. Place/aim the HC-SR04 to obtain stable echoes from the tracked target through the intended movement; it measures range along its beam, not joint angle.

In-band time credits consecutive valid in-zone samples; credited gaps are capped at 150 ms. Outside/invalid readings or pauses stop credit. The first qualifying sample earns no preceding interval. Consistency uses in-band time / (active time − last continuous departure), excluding pauses and final departure; earlier departures followed by return remain in the denominator.

## Part B chest press

Use the same distance-sensor wiring. Select Chest press with MODE. Follow its two calibration steps: hold the starting pose and long-press/release MODE to collect it, then do the same at the pressed pose. The two references must differ by at least 10 cm and be within the accepted range. Confirm both before GO starts.

A meaningful attempt leaves the start and reaches at least 30% travel. A good rep reaches 80% calibrated travel, then returns within 20% of the start with endpoint confirmation. A completed shorter attempt stays in the attempt count, lowering good-rep percentage. Extra travel is allowed. Incomplete cycles discarded on pause, finish or invalid echoes do not count as completed attempts. See [CHEST_PRESS.md](CHEST_PRESS.md).

## Part B finish, cancel, save and erase

Hold GO ≥1.2 seconds and release during RUNNING/PAUSED to finish/save. DONE shows a fixed saved-results screen. Short GO starts another session with fresh counters and retained calibration; MODE changes exercise/calibration.

To stop without saving: pause, hold MODE **2 seconds**, then GO confirms Cancel session or MODE returns. The cancellation does not erase previous history.

To erase saved B history: from IDLE/DONE hold both GO+MODE **3 seconds**, release both, then tap GO to confirm or MODE to cancel. Both hold and chest archives are cleared only after confirmation. Keep power connected until completion. The computer's profiles/CSVs remain.

Serial `history` reads up to 20 hold records; `rep_history` reads up to 10 chest records. `version` reports B2.7. The new app reads both B archives in one connection; the PowerShell exporter uses `--reps` for chest records and defaults to holds.
