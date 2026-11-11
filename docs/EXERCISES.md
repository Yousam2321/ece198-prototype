# Exercise-by-exercise guide

All commands below are separate newline-terminated messages. **Semicolons in this table
mean “then send the next command”; do not type an entire table sequence on one line.**
`select` and `teach` require IDLE. Wait through the full eight-second teaching period
before `go`. After DONE, use `reset` to return to IDLE before changing exercise.

## Wearable A (R4 Minima)

| ID | Exercise | Profile file in WearableMonitor/exercises/ | Implemented behavior | Command sequence |
|---|---|---|---|---|
| 0 | Treadmill | `Treadmill.h` | Timed warm-up and 15-minute cardio; optional candidate HR | `select 0; demo; go` |
| 1 | Thoracic spine rotations | `ThoracicRotations.h` | Timer + manual rep confirmations; no yaw angle measurement | `select 1; go; rep` |
| 2 | Airplane | `Airplane.h` | Timed hold; observer starts/ends it | `select 2; go; finish` |
| 3 | Chest press | `ChestPress.h` | Taught candidate full-cycle reps; manual fallback | `select 3; teach; go` |
| 4 | Glute bridges | `GluteBridges.h` | Taught candidate full-cycle reps; manual fallback | `select 4; teach; go` |
| 5 | Lat pulldown | `LatPulldown.h` | Taught candidate full-cycle reps; manual fallback | `select 5; teach; go` |
| 6 | Farmer's carry | `FarmersCarry.h` | Duration + manually entered metres; no force/weight measurement | `select 6; go; distance 10` |
| 7 | Hip flexor stretch | `HipFlexorStretch.h` | Timed hold; observer starts/ends it | `select 7; go; finish` |
| 8 | Cat-cow | `CatCow.h` | Taught candidate cycles; no spinal-curvature measurement | `select 8; teach; go` |

## Station B (R3)

| Exercise | File in HoldStation/exercises/ | Behavior | Command sequence |
|---|---|---|---|
| Wall sit | `WallSit.h` | Time within one calibrated distance band | `wall; cal; go` |
| Plank | `Plank.h` | Time within one calibrated distance band | `plank; cal; go` |

Wait for the two-second `cal` procedure to finish before `go`.
Switching station mode invalidates the reference. Each profile supplies its default
band tolerance. The `band 4` command adjusts the current idle session to +/-4cm.
This reference distance does not prove full-body alignment or squat depth.

## Shared controls

- `go`: start with a three-second countdown, or pause/resume a running/paused session.
- `finish`: emit a session summary and enter DONE.
- `reset`: return to IDLE; do this from PAUSED/DONE, not while RUNNING.
- Bare button taps have the same start/pause/resume behavior.
- Hold and release the wearable button during a session to finish; while IDLE it
  starts teaching only for a candidate-rep profile.
- On B, tap MODE to select wall sit/plank; hold >=1.2s and release MODE to calibrate.

## Automatic repetition trial

1. Select a supported candidate profile; secure the sensor to a repeatable placement.
2. Send `teach`. Perform approximately three comfortable full cycles over eight seconds.
3. A signal-span check chooses the largest axis and normalizes its endpoints.
   Passing this check does **not** prove rep-count accuracy.
4. Send `go`; after the countdown compare proposed counts with an observer.
5. A count requires a LOW -> HIGH -> LOW cycle, with timing and hysteresis checks.
6. Send `finish`. Repeat three 10-rep trials and record errors, placement and failures.
7. If unreliable, reset, send `manual` while IDLE, start, then send `rep` per observed rep.

Re-teach whenever placement or exercise changes. Do not exaggerate/jerk a movement to
force a passing signal. Neither acceleration nor ultrasonic distance measures muscle
activation, force, full technique or pain-free effort.

## Treadmill and optical HR

Default real timing: warm-up 180 seconds (adjustable) then cardio 900 seconds.
`demo` changes to 5+15 seconds and labels every row DEMO. `real` restores Config.h values.
Pauses and the startup countdown do not count toward active time.

After enabling verified HR hardware, keep finger contact still. `baseline` while IDLE
collects a 20-second candidate resting baseline. `goal N` compares against a goal supplied
for the demonstration; no default physiological goal is chosen. `goal 0` clears it.
The class/header MAX30105 is expected even with a MAX30102 sensor.
BPM is suppressed when contact/quality heuristics fail. No SpO2 algorithm is included.

For carry, `distance 10` means **10 metres entered by an observer**, not an automatic
GPS/accelerometer distance result. For hold/stretch exercises, `finish` records when the
observer ends a session; there is no independent automatic posture-duration detector.
