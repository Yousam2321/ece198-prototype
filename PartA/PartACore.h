#pragma once
#include <stdint.h>

// Same explicit session-state approach as Part B. All clocks use unsigned
// subtraction, so a millis() wrap does not break pause/countdown timing.
namespace parta {
enum State : uint8_t { IDLE, COUNTDOWN, RUNNING, PAUSED, SAVING, DONE };
inline const char* stateName(State s) {
  switch(s) {
    case IDLE:return "IDLE"; case COUNTDOWN:return "COUNTDOWN";
    case RUNNING:return "RUNNING"; case PAUSED:return "PAUSED";
    case SAVING:return "SAVING"; default:return "DONE";
  }
}
struct Session {
  State state=IDLE;
  uint32_t active=0,last=0,countdownAt=0;
  void reset(uint32_t now){state=IDLE;active=0;last=now;countdownAt=now;}
  void tick(uint32_t now){
    if(state==RUNNING)active+=uint32_t(now-last);
    last=now;
    if(state==COUNTDOWN && uint32_t(now-countdownAt)>=3000)state=RUNNING;
  }
  void start(uint32_t now){reset(now);state=COUNTDOWN;countdownAt=now;}
};
enum KeyEvent : uint8_t { NO_KEY, TAP, HOLD };
// One event per gesture. A long hold fires while held; releasing cannot tap.
// Suppression requires a complete release, preventing menu holds from leaking
// into START/pause/save or EXERCISE/switch/cancel actions.
struct Key {
  bool raw=false,stable=false,armed=true,held=false;
  uint32_t changedAt=0,downAt=0;
  void suppress(bool pressed,uint32_t now){
    raw=stable=pressed;armed=!pressed;held=false;changedAt=downAt=now;
  }
  KeyEvent update(bool pressed,uint32_t now,uint32_t holdMs){
    if(pressed!=raw){raw=pressed;changedAt=now;}
    if(raw!=stable && uint32_t(now-changedAt)>=30){
      stable=raw;
      if(stable){downAt=now;held=false;}
      else {
        bool tap=armed && !held;
        armed=true;held=false;
        return tap?TAP:NO_KEY;
      }
    }
    if(stable && raw && armed && !held && uint32_t(now-downAt)>=holdMs){
      held=true;return HOLD;
    }
    return NO_KEY;
  }
};
}
