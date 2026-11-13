# Part A — original squats and lateral raises, with saved history

Open `PartA/PartA.ino`; version A1.4. Both Uno R3 and Uno R4 Minima builds are checked. The uploaded working sketch is preserved in the first Part A branch commit, before integration edits.

## Detection and controls

- START D2 and EXERCISE D3, active-low with INPUT_PULLUP; 50 ms debounce.
- Short START release starts/pauses/resumes. Holding START 3000 ms ends while held (extended from the original 1 second).
- EXERCISE changes modes on release only while stopped and resets the exercise being left; delaying the switch until release prevents a two-button hold from switching first.
- Analog accelerometer X=A0, Y=A1, Z=A2; 200-sample startup baseline; 25 ms reads; gravity EMA alpha 0.05.
- Squats: Z ≤ −40 for 3 samples, then Z ≥ −5 for 8 samples. Lateral raises: Y ≥ 25 for 3 samples, then Y ≤ 5 for 8 samples. These are baseline-corrected ADC units, **not degrees**. Existing 1200 ms cooldowns remain.
- The revised team sketch raises the lateral-raise upward threshold from 20 to 25. Return threshold, confirmation counts, squat detection and normal LCD layout are preserved. A1.2 routes output through bounded single-byte serial writes and skips optional XYZ telemetry when output space is unavailable; accelerometer sampling and rep detection still run.
- Original LCD text/layout and rolling mean of up to 5 accepted BPM readings; finger threshold IR ≥50000; accepted beats 40–180 BPM. No SpO2 calculation is added.

## Pins from the original sketch

| Function | Pin(s) |
|---|---|
| START / EXERCISE | D2 / D3 to GND when pressed |
| Analog accelerometer X / Y / Z | A0 / A1 / A2 |
| LCD RS / E / D4 / D5 / D6 / D7 | D7 / D8 / D9 / D10 / D11 / D12 |
| MAX30102 SDA / SCL | Board SDA / SCL (A4 / A5 on these Uno boards) |

Keep the existing working power and LCD contrast wiring. The sketch does not establish a generic breakout's voltage compatibility. Libraries: Arduino LiquidCrystal and SparkFun MAX3010x Pulse and Proximity Sensor Library. EEPROM comes with the board core.

## Added lifecycle

| Action | Result |
|---|---|
| START from stopped | Runs/resumes the original counter; active duration accumulates |
| Short START while running | Pauses; reps/time retained; no EEPROM write |
| Long START ≥3 s | Saves the started session, then performs the original END/reset |
| EXERCISE while paused after starting | Saves the exercise being left, then performs the original switch/reset |
| EXERCISE in a never-started mode | Switches without an empty archive entry |
| Power off | Completed summaries remain; unfinished RAM session is lost |

Saving failure prints `A_EVENT,part_a,save_failed_counts_kept`, pauses the exercise and keeps its counters for retry; it does not silently claim success. The original END LCD still resets its rep display, so inspect the saved summary through the app or Serial.

## Erase history with the existing buttons

1. This menu is available before, during or after an exercise, including while paused.
2. Hold START + EXERCISE together for at least 3 seconds, until the prompt appears. Either button may be pressed first. Counting pauses as soon as the chord is detected.
3. The LCD shows `ERASE HISTORY?` / `START=YES EX=NO`. Release both buttons completely.
4. Tap/release START to erase all saved A history, or tap/release EXERCISE to cancel.
5. `HISTORY CLEARED`, `ERASE CANCELLED` or `ERASE FAILED` stays visible for 2 seconds, then the normal LCD and prior running/paused state return.

The opening hold/release cannot confirm deletion. Pressing both buttons again while confirming requires another complete release before a separate choice. The menu never starts, switches, finishes or saves an attempt. Menu time is excluded from active time. Current reps, active duration and accepted-beat statistics are retained; an incomplete movement must restart after the pause. Confirm erases saved history only, so finishing the current attempt later can add a new saved row. Cancelling leaves EEPROM byte-for-byte unchanged. Only the board's saved history is cleared; exported files and imported app profiles remain. Normal START alone still needs 3 seconds to finish/save. No new hardware is needed.

## EEPROM and structured protocol

`PartASessions.h` stores the latest 20 records, 23 bytes each at addresses 0–459. A CRC and marker written last reject interrupted/corrupt writes. A redundant generation journal uses 900–915. No EEPROM write happens in the sample loop. Generation advances on erase, making old records invisible after the erase commit even if power then fails. The oldest slot is replaced when full; an interrupted replacement can lose that oldest record, while other committed records remain.

At 115200 baud / Newline, `history` prints:

```text
# ASAVED,id,exercise,active_ms,reps,bpm_avg,bpm_samples,history_id
ASAVED,1,squats,12345,5,72.3,4,1
ASAVED,2,lateral_raises,20000,8,,0,1
# END_A_HISTORY
```

The blank BPM field means no accepted beats. This example is synthetic. Mean saved BPM uses accepted beats collected only while active, independently of the unchanged LCD's rolling mean. It does not measure heart-rate accuracy or grade exercise intensity. Active time excludes pauses. Export timestamps are computer export times, not session dates.

`version` identifies the board. `erase CONFIRM` clears saved Part A history while stopped; current RAM counters, local app profiles and CSV files remain. Stop the session before reading history. The updated app auto-detects this board and does not send B's `rep_history` command.

## Bench tests

1. Start with the original mounting; verify both detectors still count the same controlled movements.
2. Count 3 squats. Pause for 5 s, resume and do 2 more. Hold START 3 s. The saved row should have 5 reps and active time excluding the pause.
3. Start another squat session, pause, then press EXERCISE. Check that its row saved before the count cleared. Run/end lateral raises and check a distinct exercise row.
4. End without any accepted pulse beats: BPM must be unavailable. Repeat with steady finger contact: mean accepted BPM and sample count should be present. Do not grade the exercise using BPM.
5. Power-cycle **after finishing**, then Read COM port or export CSV. Completed rows must remain and duplicate imports must skip them.
6. Confirm rep goals in the app; test a goal below/equal/above the saved count. Check reasons, same-exercise progression, PDF and evaluated-CSV round trip. B results must still load.
7. After exporting needed data, try the button erase menu: cancel first and check history remains; then open it again and confirm. Verify the archive empties and the next session has a newer history generation. The app's imported rows remain. The existing Serial `erase CONFIRM` command is still available.

Host tests simulate the actual buttons, ADC input and sketch loop. They cannot establish physical rep or pulse accuracy.

## Continue using the board after export

A1.2 adds PartASerial.h. Serial output checks connection and available space before every byte and limits waits to 20 ms. This avoids the R4 USB CDC bulk-write loop, which can wait indefinitely when its FIFO fills after a host stops reading. Optional XYZ telemetry is skipped instead of holding up sampling. Buttons, LCD and EEPROM saving do not require an open host connection. A failed history transfer never sends the completion marker; reopen/retry export instead of accepting a partial CSV.

Finish/save, export from COM6, let the exporter close, then start/pause/resume/finish another session without disconnecting power. Import the new result to verify it saved. This is a user-side hardware check; simulated port-close/stall tests pass but do not prove the cause of every physical freeze.

## Revised lateral-raise data — A1.3

The supplied ECE_198_with_lat_raises(1).ino has one functional difference from the earlier team sketch: LAT_RAISE_UP_THRESHOLD changes from 20.0 to 25.0. A1.3 merges that value into the integrated sketch, retaining the 3-second finish/save hold, button-confirmed erase, EEPROM/CSV/app compatibility and bounded serial output. The return threshold remains 5.0, with 3 upward and 8 return samples and the existing 1200 ms cooldown. These are baseline-relative ADC units, not degrees.

No wiring, app, saved-record format or prior history change is required. Repeat a controlled lateral raise using the team's original mounting; compare counted reps with an observer. Whether the revised value reduces real false counts must be checked on the board.

## Erase at any session state — A1.4

A1.4 removes the previous beginning/finished-only restriction. While running, opening the menu pauses the attempt and returning resumes it; while paused it remains paused. Idle/finished states stay idle/finished. Releasing an insufficient hold returns to the prior state without erasing or saving. Existing archive format and session statistics are preserved.

## Closed-port input guard — A1.5

The R4 USB core can report buffered input after the COM port closes while `read()` returns -1 without consuming that input. A1.5 exits the input loop on a negative read, discards the partial command, and limits each loop pass to 32 input bytes so serial traffic cannot starve controls. A regression reproduces this R4 state, then exercises start, count, pause, resume, finish/save and a second export without a power cycle. The heart-rate sensor remains enabled and the lateral-raise upward threshold remains 25. Hardware confirmation of the reported freeze is still required.
