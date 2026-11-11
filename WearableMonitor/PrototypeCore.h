#pragma once
#include <stdint.h>

// Platform-independent timing/detection logic, shared by both Arduino sketches.
namespace ece198 {
enum RunState : uint8_t { IDLE, COUNTDOWN, RUNNING, PAUSED, DONE };
inline const char* stateName(RunState s) {
  switch (s) {
    case IDLE: return "IDLE";
    case COUNTDOWN: return "COUNTDOWN";
    case RUNNING: return "RUNNING";
    case PAUSED: return "PAUSED";
    default: return "DONE";
  }
}
// active is running time in milliseconds. COUNTDOWN and PAUSED never add to it.
// Subtraction uses uint32_t so a single millis() wrap is handled correctly.
struct Session {
  RunState state = IDLE;
  uint32_t active = 0, last = 0, countdownAt = 0;
  void reset(uint32_t now) { state = IDLE; active = 0; last = now; }
  void tick(uint32_t now) {
    if (state == RUNNING) active += uint32_t(now - last);
    last = now;
    if (state == COUNTDOWN && uint32_t(now - countdownAt) >= 3000) {
      state = RUNNING;  // Starts here; never credits delayed/countdown time.
    }
  }
  void go(uint32_t now) {
    tick(now);
    if (state == IDLE) { state = COUNTDOWN; countdownAt = now; }
    else if (state == RUNNING) state = PAUSED;
    else if (state == PAUSED) state = RUNNING;
    else if (state == DONE) reset(now);
  }
  void finish(uint32_t now) { tick(now); state = DONE; }
};

enum ButtonEvent : uint8_t { NO_PRESS, SHORT_PRESS, LONG_PRESS };
// Debounce for 30 ms; emit one event on release. Holds >=1200 ms are long.
// pressed is already a logical boolean: the sketch converts active-low wiring.
struct Button {
  bool raw = false, stable = false;
  uint32_t changedAt = 0, downAt = 0;
  ButtonEvent update(bool pressed, uint32_t now) {
    if (pressed != raw) { raw = pressed; changedAt = now; }
    if (raw == stable || uint32_t(now - changedAt) < 30) return NO_PRESS;
    stable = raw;
    if (stable) { downAt = now; return NO_PRESS; }
    return uint32_t(now - downAt) >= 1200 ? LONG_PRESS : SHORT_PRESS;
  }
};

// Counts a full LOW -> HIGH -> LOW cycle, with hysteresis and time limits.
// Feed a normalized calibration signal: 0 is one endpoint, 1 the other.
struct CycleCounter {
  uint8_t phase = 0;
  uint32_t at = 0;
  void reset() { phase = 0; }
  bool update(float value, uint32_t now) {
    if (phase && uint32_t(now - at) > 10000) phase = 0;
    if (phase == 0 && value <= 0.25f) { phase = 1; at = now; }
    else if (phase == 1 && value >= 0.75f && uint32_t(now - at) >= 150) phase = 2;
    else if (phase == 2 && value <= 0.25f) {
      bool counted = uint32_t(now - at) >= 600;
      phase = 1; at = now;
      return counted;
    }
    return false;
  }
};

// Credits only consecutive valid samples; dropouts and pauses break continuity.
// A fresh in-range sequence must settle for 300 ms. The first sample after a
// gap does not earn credit, and intervals >150 ms break continuity.
struct RangeTimer {
  uint32_t total = 0, last = 0, enteredAt = 0;
  bool previousInside = false;
  void clear() { total = 0; breakContinuity(); }
  void breakContinuity() { previousInside = false; }
  void update(uint32_t now, bool running, bool valid, bool inside) {
    if (!running || !valid || !inside) { breakContinuity(); last = now; return; }
    uint32_t dt = uint32_t(now - last);
    if (!previousInside || dt > 150) { enteredAt = now; previousInside = true; }
    else if (uint32_t(now - enteredAt) >= 300) total += dt;
    last = now;
  }
};
}
