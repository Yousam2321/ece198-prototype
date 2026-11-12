# Part A — original squats and lateral raises, with saved history

Open `PartA/PartA.ino`; version A1.0. Both Uno R3 and Uno R4 Minima builds are checked. The uploaded working sketch is preserved in the first Part A branch commit, before integration edits.

## What stays the same

- START D2 and EXERCISE D3, active-low with INPUT_PULLUP; 50 ms debounce.
- Short START release starts/pauses/resumes. Holding START 1000 ms ends while held.
- EXERCISE changes modes only while stopped and resets the exercise being left.
- Analog accelerometer X=A0, Y=A1, Z=A2; 200-sample startup baseline; 25 ms reads; gravity EMA alpha 0.05.
- Squats: Z ≤ −40 for 3 samples, then Z ≥ −5 for 8 samples. Lateral raises: Y ≥ 20 for 3 samples, then Y ≤ 5 for 8 samples. These are baseline-corrected ADC units, **not degrees**. Existing 1200 ms cooldowns remain.
- The full squat detector, lateral-raise detector, accelerometer update and LCD update function bodies are unchanged from the supplied sketch.
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
| Long START ≥1 s | Saves the started session, then performs the original END/reset |
| EXERCISE while paused after starting | Saves the exercise being left, then performs the original switch/reset |
| EXERCISE in a never-started mode | Switches without an empty archive entry |
| Power off | Completed summaries remain; unfinished RAM session is lost |

Saving failure prints `A_EVENT,part_a,save_failed_counts_kept`, pauses the exercise and keeps its counters for retry; it does not silently claim success. The original END LCD still resets its rep display, so inspect the saved summary through the app or Serial.

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
2. Count 3 squats. Pause for 5 s, resume and do 2 more. Hold START 1 s. The saved row should have 5 reps and active time excluding the pause.
3. Start another squat session, pause, then press EXERCISE. Check that its row saved before the count cleared. Run/end lateral raises and check a distinct exercise row.
4. End without any accepted pulse beats: BPM must be unavailable. Repeat with steady finger contact: mean accepted BPM and sample count should be present. Do not grade the exercise using BPM.
5. Power-cycle **after finishing**, then Read Arduino or export CSV. Completed rows must remain and duplicate imports must skip them.
6. Confirm rep goals in the app; test a goal below/equal/above the saved count. Check reasons, same-exercise progression, PDF and evaluated-CSV round trip. B results must still load.
7. Only after exporting needed data, use `erase CONFIRM`; verify the archive empties and the next session has a newer history generation. The app's imported rows remain.

Host tests simulate the actual buttons, ADC input and sketch loop. They cannot establish physical rep or pulse accuracy.
